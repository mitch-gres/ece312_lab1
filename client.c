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
#define MAX_LINE 256

#define SOH 0x01;
#define STX 0x02;
#define ETX 0x03;
#define LF 0x0A;
#define CR 0x0D;

// Types of messages that can be sent between client and server. 
enum msg_type {
  INTRODUCTION_MSG = 1,
  CHAT_MSG = 2,
  PROBE_MSG = 3,
  NAME_CHANGE_MSG = 4,
  GOODBYE_MSG = 5,
  ACK_MSG = 6
};

#define MAX_NAME_LEN 100
#define MAX_MSG_LEN 256
typdef struct {
  const char soh_0;
  const char stx_0;
  uint8_t sequence_number;
  const char etx_0;
  enum msg_type type;
  const char stx_1;
  char name[MAX_NAME_LEN];
  const char etx_1;
  const char stx_2;
  char message[MAX_MSG_LEN];
  const char etx_2;
  const char cr;
  const char lf;
} message_t;

message_t create_message(enum msg_type type, const char *name, const char *message, uint8_t sequence_number) {
  message_t msg;
  msg.soh_0 = SOH;
  msg.stx_0 = STX;
  msg.sequence_number = sequence_number;
  msg.etx_0 = ETX;
  msg.type = type;
  msg.stx_1 = STX;
  strncpy(msg.name, name, MAX_NAME_LEN);
  msg.etx_1 = ETX;
  msg.stx_2 = STX;
  strncpy(msg.message, message, MAX_MSG_LEN);
  msg.etx_2 = ETX;
  msg.cr = CR;
  msg.lf = LF;
  return msg;
}

int
main(int argc, char * argv[]){
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
  }
	close(fd);
}