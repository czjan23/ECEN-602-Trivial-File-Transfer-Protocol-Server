#include "message.h"

// tftp error code
static const char *tftp_error_messages[] = {
   "Undefined error",                 // Error code 0
   "File not found",                  // 1
   "Access violation",                // 2
   "Disk full or allocation error",   // 3
   "Illegal TFTP operation",          // 4
   "Unknown transfer ID",             // 5
   "File already exists",             // 6
   "No such user"                     // 7
};

// error handler
void print_err_and_exit(char *err_message) {
   fprintf(stdout, "Error: %s\n", err_message);
   exit(1);
}

void debug(const char *data, int len) {
   int i = 0;

   for(i = 0; i < len; i++) {
      printf("%02x ", data[i]);
   }

   printf("\n");
}

// send message
void send_msg(connection_t *connP) {
   sendto(connP->sock, connP->data, connP->len, MSG_DONTWAIT, (struct sockaddr *)&connP->addr, connP->addrLen);
}

// send file data
void send_data(connection_t *connP, short block_num) {
   char buffer[MAX_MESSAGE_SIZE] = {0};
   size_t count = 0;

   short *p = (short *)buffer;

   *p = htons(DATA);

   p++; // offset = sizeof(short)


   if(connP->fp == NULL) {
      // handle last ACK from client
      printf("Final ACK has been received\n");
      printf("Client Disconnected\n");
      close(connP->sock);
      exit(EXIT_SUCCESS);
   }

   if(connP->ack != block_num) {
      // check if ACK if same as it was sent
      printf("send_data ack is err\n");
      return;
   }

   bzero(p, MAX_MESSAGE_SIZE - sizeof(short));
   connP->ack = block_num + 1;
   *p = htons(connP->ack);

   char nextchar = -1;
   char c;
   char *q = (char *)(p + 1);

   if(memcmp(&(connP->mode), MODE_NETASCII, strlen(MODE_NETASCII)) == 0) {
      // handle netascii file 
      for(count = 0; count < DEFAULT_PACKET_SIZE; count++) {
         if(nextchar >= 0) {
            *q++ = nextchar;
            nextchar = -1;
            continue;
         }

         c = getc(connP->fp);

         if(c == EOF) {
            if(ferror(connP->fp)) {
               printf("send_data read err from getc on local file\n");
            }
            break;
         } else if(c == '\n') {
            c = '\r';
            nextchar = '\n';
         } else if(c == '\r') {
            nextchar = '\0';
         }else {
            nextchar = -1;
         }

         *q++ = c;
      }
   } else {
      // handle octet file
      count = fread(q, sizeof(char), DEFAULT_PACKET_SIZE, connP->fp);
   }

   if(count > 0) {
      // send valid data
      connP->len = count + 2 * sizeof(short);
      memcpy(connP->data, buffer, connP->len);
      //printf("send_data data_len %d\n", connP->len);
      //debug(buffer, connP->len);
      send_msg(connP);
   } else if(count == 0) {
      // come to the end of file, send last empty data
      connP->len = 2 * sizeof(short);
      memcpy(connP->data, buffer, connP->len);
      printf("Block with zero bytes read\n");
      send_msg(connP);
   }

   if(count < DEFAULT_PACKET_SIZE) {
      // number of bytes less than 512, considered as last package
      if(connP->fp != NULL) {
         fclose(connP->fp);
         connP->fp = NULL;
      }
      connP->ack = 0;
      printf("Block with less than 512 bytes read\n");
   }
}

// send RRQ message
void send_rrq(connection_t *connP, const char *file_name) {
   char buffer[64] = {0};

   short *p = (short *)buffer;

   *p = htons(RRQ);

   p++; // offset = sizeof(short)

   char *q = (char *)p;

   memcpy(q, file_name, strlen(file_name));

   q += strlen(file_name) + 1;

   memcpy(q, MODE_OCTET, strlen(MODE_OCTET));

   connP->fp = fopen(file_name, "wb");

   if(connP->fp == NULL) {
      printf("rq_action fopen err %s\n", strerror(errno));
      return;
   }

   connP->len = sizeof(short) + strlen(file_name) + 1 + strlen(MODE_OCTET) + 1;
   memcpy(connP->data, buffer, connP->len);
   //debug(buffer, connP->len);
   send_msg(connP);
}

// send WRQ message
void send_wrq(connection_t *connP, const char *file_name) {
   char buffer[64] = {0};

   short *p = (short *)buffer;

   *p = htons(WRQ);

   p++; // offset = sizeof(short)

   char *q = (char *)p;

   memcpy(q, file_name, strlen(file_name));

   q += strlen(file_name) + 1;

   memcpy(q, MODE_OCTET, strlen(MODE_OCTET));

   connP->fp = fopen(file_name, "rb");

   if(connP->fp == NULL) {
      printf("rq_action fopen err %s\n", strerror(errno));
      return;
   }

   connP->len = sizeof(short) + strlen(file_name) + 1 + strlen(MODE_OCTET) + 1;
   memcpy(connP->data, buffer, connP->len);
   //debug(buffer, connP->len);
   send_msg(connP);
}

// send ACK message
void send_ack(connection_t *connP, short block_num) {
   char buffer[64] = {0};

   short *p = (short *)buffer;

   printf("ACK No sent is %d\n", block_num);

   *p = htons(ACK);

   p++; // offset = sizeof(short)

   *p = htons(block_num);

   connP->len = 2 * sizeof(short);
   memcpy(connP->data, buffer, connP->len);
   //debug(buffer, connP->len);
   send_msg(connP);
}

// send error message
void send_error(connection_t *connP, short err_code, const char *error) {
   char buffer[64] = {0};

   short *p = (short *)buffer;

   *p = htons(ERROR);

   p++; // offset = sizeof(short)

   *p = htons(err_code);

   memcpy(p + 1, error, strlen(error));

   connP->len = 2 * sizeof(short) + strlen(error);
   memcpy(connP->data, buffer, connP->len);
   //debug(buffer, connP->len);
   send_msg(connP);
}

// receive and handle data
void data_action(connection_t *connP, const char *data, ssize_t len) {
   short block_num = 0;
   size_t count = 0;

   short *p = (short *)data;

   if(connP->fp == NULL)
   {
      return;
   }

   block_num = ntohs(*p);

   ssize_t data_len = len - sizeof(short);
   printf("%zu no of bytes read\n", data_len);

   count = fwrite(data + sizeof(short), sizeof(char), data_len, connP->fp);

   printf("Packet No received is %d\n", block_num);
   send_ack(connP, block_num); //send back ACK

   if(count < DEFAULT_PACKET_SIZE) {
      // if length of data less than 512 bytes, regarded as last pack
      if(connP->fp != NULL) {
         fclose(connP->fp);
         connP->fp = NULL;
      }

      printf("Last Block with less than 512 bytes data received\n");
      printf("Client Disconnected\n");
      close(connP->sock);
      exit(EXIT_SUCCESS); // exit child process
   }
}

// hand RRQ/WRQ message
void rq_action(connection_t *connP, const char *data, ssize_t len) {
   char file_name[64] = {0};
   char *p = (char *)data;
   int i = 0;

   // resolve file name
   while(1) {
      file_name[i++] = *p;
      if(*p++ == 0) {
         break;
      }
   }

   //printf("rq_action file_name %s\n", file_name);

   i = 0;
   memset(connP->mode, 0, sizeof(connP->mode));

   // resolve mode info
   while(1) {
      connP->mode[i++] = *p;
      if(*p++ == 0) {
         break;
      }
   }

   //printf("rq_action mode %s\n", connP->mode);

   if(connP->opcode == WRQ) {
      // handle WRQ
      printf("WRQ packet received\n");
      connP->fp = fopen(file_name, "wb");

      if(connP->fp == NULL) {
         printf("rq_action fopen err %s\n", strerror(errno));
         return;
      }
      send_ack(connP, WRQ_FIRST_RESPOND); //special ack
   }
   else if(connP->opcode == RRQ) {
      // handle RRQ 
      connP->fp = fopen(file_name, "rb");

      if(connP->fp == NULL) {
         printf("rq_action fopen err %s\n", strerror(errno));
         send_error(connP, 1, tftp_error_messages[1]);
         return;
      }

      send_data(connP, RRQ_FIRST_ACK); //special ack
   }
}

// handle ACK
void ack_action(connection_t *connP, const char *data, ssize_t len) {
   short block_num = 0;
   short *p = (short *)data;
   block_num = ntohs(*p);
   //printf("ack_action block_num %d\n", block_num);

   send_data(connP, block_num); //收到ACK, 继续发送文件
}

// handle error message
void error_action(const char *data, ssize_t len) {
   short error_num = 0;
   short *p = (short *)data;
   error_num = ntohs(*p);
   printf("error_action error_num %d\n", error_num);
   printf("error_action error_msg %s\n", data + sizeof(short));
}

// handle client request
void connect_data(connection_t *connP, const char *data, ssize_t len) {
   //debug(data, len);
   short *p = (short *)data;
   connP->opcode = ntohs(*p);

   switch(connP->opcode) {
      case RRQ:
      case WRQ:
         rq_action(connP, data + sizeof(short), len - sizeof(short));
         break;

      case DATA:
         data_action(connP, data + sizeof(short), len - sizeof(short));
         break;

      case ACK:
         ack_action(connP, data + sizeof(short), len - sizeof(short));
         break;

      case ERROR:
         error_action(data + sizeof(short), len - sizeof(short));
         break;

      default:
         printf("connect_data opcode err %d\n", connP->opcode);
         send_error(connP, 5, tftp_error_messages[5]);
         break;
   }
}

// create udp connection
int new_server(connection_t *connP, const char *ip, int port, char flag) {
   int socket_fd = socket(AF_INET, SOCK_DGRAM, 0); //not SOCK_STREAM

   if(socket_fd < 0) {
      print_err_and_exit("socket error!");
   }

   // init father socket sockaddr_in
   struct sockaddr_in server_addr;
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(port);

   if(flag == SERVER_FLAG) {
      // it's server
      server_addr.sin_addr.s_addr = INADDR_ANY;

      // bind socket
      if(bind(socket_fd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0) {
         print_err_and_exit("bind error!");
      }

      unsigned int addr_len = sizeof(struct sockaddr);

      if(getsockname(socket_fd, (struct sockaddr *)&server_addr, &addr_len) == -1) {
         perror("getsockname");
         exit(1);
      }

      //printf("new_server on port %i\n", ntohs(server_addr.sin_port));
   } else {
      // it's client
      inet_pton(AF_INET, ip, &server_addr.sin_addr);
      connP->addrLen = sizeof(server_addr);
      memcpy(&connP->addr, &server_addr, connP->addrLen);
   }

   return socket_fd;
}
