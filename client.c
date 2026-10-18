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
#include <pthread.h>

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

  if (msg.soh != SOH || msg.cr != CR || msg.lf != LF) {
    fprintf(stderr, "Invalid message format: SOH, CR, or LF not found.\r\n");
    msg.soh = 0xFF; // Indicates an invalid message. 
  }
return msg;
}

void* foo(void* arg){
	char buf[MAX_LINE];
	int fd = open("/tmp/myfifo", O_RDONLY);
	

	if (fd == -1) {
		perror("open");
		return NULL;
	}

	ssize_t n;
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		message_t rx_msg = get_message_from_buffer(buf);
    if (rx_msg.soh != 0xFF) {
      switch(rx_msg.msg_type) {
        case INTRODUCTION_MSG:
          fprintf(stdout, "Received Introduction from %s. \n\r", rx_msg.name);
          break;
        case CHAT_MSG:
          fprintf(stdout, "Received Chat from %s: %s\r\n", rx_msg.name, rx_msg.body);
          break;
        case PROBE_MSG:
          fprintf(stdout, "Received Probe from %s.\r\n", rx_msg.name);
          break;
        case NAME_CHANGE_MSG:
          fprintf(stdout, "Received Name Change from %s.\r\n", rx_msg.name);
          break;
        case GOODBYE_MSG:
          fprintf(stdout, "Received Goodbye from %s.\r\n", rx_msg.name);
          break;
        case ACK_MSG:
          fprintf(stdout, "Received ACK from %s.\r\n", rx_msg.name);
          break;
        default:
          fprintf(stdout, "Received Unknown Message Type from %s.\r\n", rx_msg.name);
	    }
    }
    else if (rx_msg.soh == 0xFF) {
      fprintf(stderr, "Message Received is invalid. Invalid message format.\r\n");
    }
		//fwrite(buf, 1, n, stdout);
		fflush(stdout);
	}

	if (n < 0)
		perror("read");

	close(fd);
	return NULL;
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
	
	
	
//	fprintf(stdout, "Enter IPV4 address of target or just wait for request");
//	fflush(stdout);
//	char hostbuff[20];
//	fgets(hostbuff, sizeof(hostbuff), stdin);
//	strncpy(host, hostbuff, 16)
	
	
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
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		perror("simplex-talk: connect");
		close(s);
		exit(1);
	}

	fd = open(myfifo, O_RDONLY);
	pthread_t thread;
	pthread_create(&thread, NULL, foo, NULL);
	if (fd == -1) {
		perror("open");
	}

	/* main loop: get and send lines of text */
	while (1) {
		if(fgets(buf, sizeof(buf), stdin)){
			buf[MAX_LINE-1] = '\0';
			len = strlen(buf) + 1;
			send(s, buf, len, 0);
		}
		
		// // We have the buf_fifo, we'll convert into a message_t struct.
		// message_t msg;
		// memcpy(&msg, buf_fifo, sizeof(message_t));
		// printf("Message Type: %d\n", msg.msg_type);
		// printf("Sequence Number: %d\n", msg.sequence_number);
		// printf("Name: %s\n", msg.name);
		// printf("Body: %s\n", msg.body);
	}
	pthread_join(thread, NULL);
	close(fd);
}