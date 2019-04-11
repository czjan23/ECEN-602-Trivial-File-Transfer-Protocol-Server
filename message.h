#ifndef MESSAGE_H__
#define MESSAGE_H__

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <libgen.h>
#include <sys/select.h>
#include "list.h"

#define MAX_MESSAGE_SIZE 1024 // max receiving message size

typedef enum {
   RRQ = 1,
   WRQ = 2,
   DATA = 3,
   ACK = 4,
   ERROR = 5,
} opcode_t;

#define MODE_NETASCII "netascii"
#define MODE_OCTET "octet"
#define MODE_MAIL "mail"

#define WRQ_FIRST_RESPOND 0
#define RRQ_FIRST_ACK 0
#define DEFAULT_PACKET_SIZE 512 // default message length

#define MAX_TIMEOUT_COUNT 10 //max number of timeout

#define SERVER_FLAG 1 //server side
#define CLIENT_FLAG 2 //client side

// struct for list
typedef struct _connection_t {
   struct list_head list;
   int sock;
   struct sockaddr_in6 addr;
   size_t addrLen;

   short ack;
   short opcode;
   char mode[64];
   char data[MAX_MESSAGE_SIZE]; //sent message
   short len; //length of sent message
   FILE *fp;
} connection_t;

void print_err_and_exit(char *err_message);
void debug(const char *data, int len);
void send_msg(connection_t *connP);
void send_data(connection_t *connP, short block_num);
void send_rrq(connection_t *connP, const char *file_name);
void send_wrq(connection_t *connP, const char *file_name);
void send_ack(connection_t *connP, short block_num);
void send_error(connection_t *connP, short err_code, const char *error);
void data_action(connection_t *connP, const char *data, ssize_t len);
void rq_action(connection_t *connP, const char *data, ssize_t len);
void ack_action(connection_t *connP, const char *data, ssize_t len);
void error_action(const char *data, ssize_t len);
void connect_data(connection_t *connP, const char *data, ssize_t len);
int new_server(connection_t *connP, const char *ip, int port, char flag);

#endif
