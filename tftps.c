#include "message.h"

static connection_t connList_head; //head of list

// look up current connection in list
int connection_find(struct sockaddr_in *addr, socklen_t addrLen) {
   struct list_head *pos;
   connection_t *connP;

   if(!list_empty(&(connList_head.list))) {
      list_for_each(pos, &(connList_head.list)) {
         connP = list_entry(pos, connection_t, list);

         if((connP->addrLen == addrLen)
            && (memcmp(&(connP->addr), addr, addrLen) == 0)) {
            return 1;
         }
      }
   }

   return 0;
}

//handle new client connection
void new_connection_hanlder(int socket, struct sockaddr_in *addr, size_t length) {
   connection_t *connection;
   connection = (connection_t *)malloc(sizeof(connection_t));

   if(connection != NULL) {
      // add new client to list
      connection->sock = socket;
      memcpy(&(connection->addr), addr, length);
      connection->addrLen = length;
      list_add_tail(&(connection->list), &(connList_head.list));
   }
}

// release list's space
void release_connection() {
   struct list_head *pos, *next;
   connection_t *connP;

   list_for_each_safe(pos, next, &(connList_head.list)) {
      connP = list_entry(pos, connection_t, list);
      list_del_init(pos);
      free(connP);
      connP = NULL;
   }
}

// delete a node from list
void delete_connection_from_list(int sock) {
   struct list_head *pos, *next;
   connection_t *connP;

   list_for_each_safe(pos, next, &(connList_head.list)) {
      connP = list_entry(pos, connection_t, list);

      // remove the connection from list
      if(connP->sock == sock) {
         list_del_init(pos);
         free(connP);
         connP = NULL;
      }
   }
}

// disconnect
void disconnect(struct sockaddr_in *addr) {
   struct list_head *pos, *next;
   connection_t *connP;

   list_for_each_safe(pos, next, &(connList_head.list)) {
      connP = list_entry(pos, connection_t, list);

      if(memcmp(&(connP->addr), addr, connP->addrLen) == 0) {
         // client disconnect
         close(connP->sock);

         char s[INET6_ADDRSTRLEN] = {0};
         s[0] = 0;
         struct sockaddr_in *saddr = (struct sockaddr_in *)&connP->addr;
         inet_ntop(AF_INET, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
         fprintf(stdout, "disconnect from [%d][%s]\n", connP->sock, s);

         // remove this connection from list
         list_del_init(pos);
         free(connP);
         connP = NULL;
         break;
      }
   }
}

// child process handler
void wait_child(int sig) {
   int status = 0;
   pid_t pid = 0;

   while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
      //printf("%d exit status is %d\n", pid, WEXITSTATUS(status));
   }
}

// child process recrive request from client
static void child_fd_isset(connection_t *connP) {
   struct sockaddr_in connection_addr = {0};
   socklen_t connection_addr_len = INET6_ADDRSTRLEN;
   char buffer[MAX_MESSAGE_SIZE] = {0};
   ssize_t numBytes = 0;

   numBytes = recvfrom(connP->sock, buffer, MAX_MESSAGE_SIZE, 0, (struct sockaddr *)&connection_addr, &connection_addr_len);

   if(numBytes == -1) {
      fprintf(stderr, "Error in recvfrom(): %d\n", errno);
   } else  {
      char s[INET6_ADDRSTRLEN];
      in_port_t port;

      s[0] = 0;
      struct sockaddr_in *saddr = (struct sockaddr_in *)&connection_addr;
      inet_ntop(AF_INET, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
      port = saddr->sin_port;
      //fprintf(stdout, "child_fd_isset %zd bytes received from [%s]:%hu\n", numBytes, s, ntohs(port));

      connP->addrLen = connection_addr_len;
      memcpy(&connP->addr, &connection_addr, connP->addrLen);
      connect_data(connP, buffer, numBytes); //handle client request
   }
}

// create child process, rebind server temp port info
void fork_child(struct sockaddr_in *addr, socklen_t addr_len, const char *data, ssize_t len) {
   int pid = fork();

   if(pid == 0) {
      //fprintf(stdout, "fork_child %d\n", getpid());

      connection_t conn = {0};
      struct timeval tv;
      fd_set readfds;
      int result;
      int count = 0;

      conn.sock = new_server(&conn, NULL, 0, SERVER_FLAG);

      conn.addrLen = addr_len;
      memcpy(&conn.addr, addr, addr_len);

      connect_data(&conn, data, len); //first

      while(1) {
         // confiure monitor
         FD_ZERO(&readfds);
         FD_SET(conn.sock, &readfds);

         tv.tv_sec = 1;
         tv.tv_usec = 0;

         // check new message
         result = select(FD_SETSIZE, &readfds, 0, 0, &tv);

         if(result < 0) {
            if(errno != EINTR) {
               fprintf(stderr, "Error in select(): %d\n", errno);
            }
         } else if(result > 0) {
            count = 0;

            // handle new message
            if(FD_ISSET(conn.sock, &readfds)) {
               child_fd_isset(&conn);
            }
         } else {
            if(conn.opcode != ERROR && conn.fp != NULL) {
               // handle timeout
               count++;
               fprintf(stderr, "Timeout has occured\n");

               if(count >= MAX_TIMEOUT_COUNT) {
                  // reach max number of timeout
                  fprintf(stderr, "Max No of timeouts occured\n");

                  close(conn.sock);
                  fclose(conn.fp);
                  exit(EXIT_SUCCESS);
               }

               send_msg(&conn); // retransmission
            }
         }
      }

      close(conn.sock);
      exit(EXIT_SUCCESS);
   }
}

// receiving new connecion
static void socket_fd_isset(int socket_fd) {
   struct sockaddr_in connection_addr = {0};
   socklen_t connection_addr_len = INET6_ADDRSTRLEN;
   char buffer[MAX_MESSAGE_SIZE] = {0};
   ssize_t numBytes = 0;

   numBytes = recvfrom(socket_fd, buffer, MAX_MESSAGE_SIZE, 0, (struct sockaddr *)&connection_addr, &connection_addr_len);

   if(numBytes == -1) {
      fprintf(stderr, "Error in recvfrom(): %d\n", errno);
   } else {
      char s[INET6_ADDRSTRLEN];
      in_port_t port;

      s[0] = 0;
      struct sockaddr_in *saddr = (struct sockaddr_in *)&connection_addr;
      inet_ntop(AF_INET, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
      port = saddr->sin_port;
      fprintf(stdout, "Server got connection from: %s and port: %hu\n", s, ntohs(port));

      int conn = connection_find(&connection_addr, connection_addr_len); // lookup current list, check if contains this conn

      if(conn != 1) {
         // add new connection to list
         new_connection_hanlder(socket_fd, &connection_addr, connection_addr_len); // add new conn to the list

         fork_child(&connection_addr, connection_addr_len, buffer, numBytes); // fork child process to handle each conn request
      }
   }
}

int main(int argc, char **argv) {
   // check parameters
   if(argc != 3) {
      print_err_and_exit("usage: echos ip port!");
   }

   fd_set readfds;
   int result;

   // init list
   INIT_LIST_HEAD(&(connList_head.list));

   int socket_fd = new_server(NULL, argv[1], atoi(argv[2]), SERVER_FLAG);

   signal(SIGCHLD, wait_child); // handle child process

   printf("Server is waiting for connections...\n");

   while(1) {
      // confiure monitor
      FD_ZERO(&readfds);
      FD_SET(socket_fd, &readfds);

      // check new message
      result = select(FD_SETSIZE, &readfds, 0, 0, NULL);

      if(result < 0) {
         if(errno != EINTR) {
            fprintf(stderr, "Error in select(): %d\n", errno);
         }
      } else if(result > 0) {
         // handle new message
         if(FD_ISSET(socket_fd, &readfds)) {
            socket_fd_isset(socket_fd);
         }
      }
   }

   close(socket_fd);
   release_connection();
}
