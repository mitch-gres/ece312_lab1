#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>

#define SERVER_PORT 710
#define MAX_LINE 512

#define SOH 1
#define LF 10
#define CR 13

// Types of messages that can be sent between client and server. 
#define INTRODUCTION_MSG 1
#define CHAT_MSG 2
#define PROBE_MSG 3
#define NAME_CHANGE_MSG 4
#define GOODBYE_MSG 5
#define ACK_MSG 6

#define MAX_NAME_LEN 100
#define MAX_MSG_LEN 256
typedef struct {
  char soh;
  uint32_t sequence_number;
  uint8_t msg_type;
  char name[MAX_NAME_LEN];
  char body[MAX_MSG_LEN];
  char cr;
  char lf;
} message_t;

message_t create_message(char msg_type, const char *name, const char *body, uint8_t sequence_number) {
  message_t msg;
  msg.soh = SOH;
  msg.sequence_number = sequence_number;
  msg.msg_type = msg_type;
  msg.cr = CR;
  msg.lf = LF;
  strncpy(msg.name, name, MAX_NAME_LEN);
  strncpy(msg.body, body, MAX_MSG_LEN);
  return msg;
}

message_t get_message_from_buffer(const char *buffer) {
  message_t msg;
  memcpy(&msg, buffer, sizeof(message_t));
  return msg;
}

int main(int argc, char * argv[]){
  FILE *fp;
  struct hostent *hp;
  struct sockaddr_in sin;
  char *host;
  char buf[MAX_LINE];
  int s;
  int len;
  
	int fd;
	char * myfifo = "/tmp/myfifo";
	char buf_fifo[MAX_LINE];

  if (argc==2) {
    host = argv[1];
  }
  else {
    fprintf(stderr, "usage: simplex-talk host\n");
    exit(1);
  }

  /* translate host name into peer's IP address */
  hp = gethostbyname(host);
  if (!hp) {
    fprintf(stderr, "simplex-talk: unknown host: %s\n", host);
    exit(1);
  }

  /* build address data structure */
  bzero((char *)&sin, sizeof(sin));
  sin.sin_family = AF_INET;
  bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
  sin.sin_port = htons(SERVER_PORT);

  /* active open */
  if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("simplex-talk: socket");
    exit(1);
  }
  if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
  {
    perror("simplex-talk: connect");
    close(s);
    exit(1);
  }
	
	fd = open(myfifo, O_RDONLY);
  /* main loop: get and send lines of text */

  char name[MAX_NAME_LEN];
  fprintf(stderr, "Enter Name: ");
  fgets(name, sizeof(name), stdin);

  // We'll make all messages chat messages for now, then change them to the appropriate type later.

  while (true) {
    fprintf(stderr, "Begin Chat...\n");
    fgets(buf, sizeof(buf), stdin);

    // Check to make sure the length of the message that we just sent is less than the maximum message length. If it is, then we can send it. If not, then we need to truncate it and send it.
    if (strlen(buf) > MAX_MSG_LEN) {
      exit(1); // Kill it. Not the best strategy, but it works for now.
    }

    message_t tx_msg = create_message(CHAT_MSG, name, buf, 0);
    send(s, &tx_msg, sizeof(tx_msg), 0);

    read(fd, buf_fifo, MAX_LINE);

    message_t rx_msg = get_message_from_buffer(buf_fifo);
    switch(rx_msg.msg_type) {
      case INTRODUCTION_MSG:
        fprintf(stderr, "Received Introduction from %s. \n\r", rx_msg.name);
        break;
      case CHAT_MSG:
        fprintf(stderr, "Received Chat from %s: %s\r\n", rx_msg.name, rx_msg.body);
        break;
      case PROBE_MSG:
        fprintf(stderr, "Received Probe from %s.\r\n", rx_msg.name);
        break;
      case NAME_CHANGE_MSG:
        fprintf(stderr, "Received Name Change from %s.\r\n", rx_msg.name);
        break;
      case GOODBYE_MSG:
        fprintf(stderr, "Received Goodbye from %s.\r\n", rx_msg.name);
        break;
      case ACK_MSG:
        fprintf(stderr, "Received ACK from %s.\r\n", rx_msg.name);
        break;
      default:
        fprintf(stderr, "Received Unknown Message Type from %s.\r\n", rx_msg.name);
    }

  }
	close(fd);
}