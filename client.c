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
const char INTRODUCTION_MSG = 1;
const char  CHAT_MSG = 2;
const char  PROBE_MSG = 3;
const char  NAME_CHANGE_MSG = 4;
const char  GOODBYE_MSG = 5;
const char  ACK_MSG = 6;

#define MAX_NAME_LEN 100
#define MAX_MSG_LEN 256
typedef struct {
  char soh;
  uint32_t sequence_number;
  char msg_type;
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
  while (fgets(buf, sizeof(buf), stdin)) {
    buf[MAX_LINE-1] = '\0';
    len = strlen(buf) + 1;
    send(s, buf, len, 0);
	
		
		read(fd, buf_fifo, MAX_LINE);
		printf("Received: %s\n", buf_fifo);

    // // We have the buf_fifo, we'll convert into a message_t struct.
    // message_t msg;
    // memcpy(&msg, buf_fifo, sizeof(message_t));
    // printf("Message Type: %d\n", msg.msg_type);
    // printf("Sequence Number: %d\n", msg.sequence_number);
    // printf("Name: %s\n", msg.name);
    // printf("Body: %s\n", msg.body);
  }
	close(fd);
}