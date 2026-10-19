#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <poll.h>

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

// Define the states of our state machine for the client.
// There are more states defined in the state machine diagram that
// we created but they're outside of this program.
#define INIT 1
#define NAME 2
#define INTRO_ACK 3
#define INTRO_WAIT 4
#define CHAT 5
#define CHAT_ACK 6
#define CHAT_WAIT 7
#define GOODBYE_ACK 8
#define GOODBYE_WAIT 9
#define PROBE_ACK 10
#define PROBE_WAIT 11
#define CHANGE_NAME_ACK 13
#define CHANGE_NAME_WAIT 14

#define MAX_NAME_LEN 100
#define MAX_MSG_LEN 256

#define TIMEOUT_SECONDS 5


typedef struct {
	char soh;
	uint32_t sequence_number;
	uint8_t msg_type;
	char name[MAX_NAME_LEN];
	char body[MAX_MSG_LEN];
	char cr;
	char lf;
} message_t;

message_t create_message(char msg_type, const char *name, const char *body, uint8_t sequence_number){
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

message_t get_message_from_buffer(const char *buffer){
	message_t msg;
	memcpy(&msg, buffer, sizeof(message_t));

	if (msg.soh != SOH || msg.cr != CR || msg.lf != LF) {
		fprintf(stderr, "Invalid message format: SOH, CR, or LF not found.\r\n");
		msg.soh = 0xFF; // Indicates an invalid message. 
	}
	return msg;
}

void *foo(void *arg){
	char buf[MAX_LINE];
	int fd = open("/tmp/myfifo", O_RDONLY);
	int fd_2;
	char *otherfifo = "/tmp/otherfifo";
	
	mkfifo(otherfifo, 0666);
	fd_2 = open(otherfifo, O_WRONLY);

	if (fd == -1){
		fprintf(stdout, "fifo open error\n");
		return NULL;
	}

	ssize_t n;

	while ((n = read(fd, buf, sizeof(buf))) > 0){
		if (write(fd_2, buf, sizeof(buf)) == -1) {
				fprintf(stdout, "fifo write error\n");
				break;
			}
		message_t rx_msg = get_message_from_buffer(buf);
		if (rx_msg.soh != 0xFF) {
			switch(rx_msg.msg_type) {
				case INTRODUCTION_MSG:
					fprintf(stdout, "Received Introduction from %s: %s. \n\r", rx_msg.body, rx_msg.name);
					break;
				case CHAT_MSG:
					//fprintf(stdout, "Received Chat from %s: %s\r\n", rx_msg.name, rx_msg.body);
					fprintf(stdout, "%s: %s\r\n", rx_msg.name, rx_msg.body);
					break;
				case PROBE_MSG:
					fprintf(stdout, "Received Probe from %s.\r\n", rx_msg.name);
					break;
				case NAME_CHANGE_MSG:
					fprintf(stdout, "Received Name Change from %s.\r\n", rx_msg.name);
					fprintf(stdout, "%s's name is now: %s.\r\n", rx_msg.name, rx_msg.body);
					break;
				case GOODBYE_MSG:
					fprintf(stdout, "Received Goodbye from %s.\r\n", rx_msg.name);
					break;
				case ACK_MSG:
					fprintf(stdout, "Received ACK from %s. Body: %s.\r\n", rx_msg.name, rx_msg.body);
					break;
				default:
					fprintf(stdout, "Received Unknown Message Type from %s.\r\n", rx_msg.name);
			}
		} else if (rx_msg.soh == 0xFF) {
			fprintf(stdout, "Message Received is invalid. Invalid message format.\r\n");
		}
		//fwrite(buf, 1, n, stdout);
		fflush(stdout);
	}

	if (n < 0)
		fprintf(stdout, "fifo read error \n");

	close(fd);
	return NULL;
}

int main(int argc, char *argv[]){
	FILE *fp;
	struct hostent *hp;
	struct sockaddr_in sin;
	char host[20]; // The other person that I'm talking to.

	char tx_buf[MAX_LINE];
	char rx_buf[MAX_LINE];
	int s;
	int len;

	int fd;
//	char *myfifo = "/tmp/myfifo";
	char *myfifo = "/tmp/otherfifo";
	char buf_fifo[MAX_LINE];
	int state = 0;
	char me_host[20];
	uint32_t seq_num = 0;
	
	struct pollfd pfds;
	short revents;
	
	
	pthread_t thread;
	pthread_create(&thread, NULL, foo, NULL);
	
	fd = open(myfifo, O_RDONLY);
	
	if (fd == -1){
		perror("open");
	}
	
	pfds.fd = fd;
	pfds.events = POLLIN;
	
	char name[MAX_NAME_LEN];
	fprintf(stdout, "Enter Name: ");
	fflush(stdout);
	fgets(name, sizeof(name), stdin);
	state = INIT;
	name[strcspn(name, "\n")] = 0;
	
	fprintf(stdout, "Enter your IP: ");
	fflush(stdout);
	fgets(me_host, sizeof(me_host), stdin);
	me_host[strcspn(me_host, "\n")] = 0;

	state = NAME;
	fprintf(stdout, "Enter hostname (IPv4 address) of target or ! to wait for introduction: ");
	fflush(stdout);	
	// Read in the IPV4 Address that's requested from the user. 
	fgets(host, sizeof(host), stdin);
	host[strcspn(host, "\n")] = 0; // fgets(...) will copy in \n so we should strip it out. 

	// If user entered a '!' instead of an IP Address then we should wait for a conncetion.
	if (host[0] == '!'){
		fprintf(stdout, "Waiting for introduction message.\r\n");
		fflush(stdout);
		state = INTRO_ACK;
		ssize_t n;
		// Monitors the FIFO if we get a message that is in the format
		// of a introduction message then we should deal with it. 
		while ((n = read(fd, rx_buf, sizeof(rx_buf))) > 0) {
			message_t rx_msg = get_message_from_buffer(rx_buf);
			if(rx_msg.msg_type == INTRODUCTION_MSG){
				strncpy(host, rx_msg.body, 20);
				//char *thing = malloc(MAX_MSG_LEN+MAX_NAME_LEN);
				//strncpy(thing, rx_msg.name);
				//strcat(thing, rx_msg.body);
				//fprintf(stdout, "Recieved Intoduction from %s: %s.\r\n", rx_msg.body, rx_msg.name);
				//fflush(stdout);
				break;
			}
		}
		
	}else{
		// If we're here then we've been given an IP address and we should assume that it's in
		// the host variable.
		state = INTRO_WAIT;
	}

	// Turns this host IP address that we got into a structure that contains a bunch of
	// information about the dude that just connected.  
	hp = gethostbyname(host);
	if (!hp){
		fprintf(stderr, "Error Unknown Host: %s\n", host);
		fprintf(stderr, "Restart Client. Program Termination\n");
		exit(1);
	}

	// Build address data structure. 
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
	sin.sin_port = htons(SERVER_PORT);

	/* active open */
	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0){
		fprintf(stderr, "Error with Socket Creation: %s\n", host);
		fprintf(stderr, "Restart Client. Program Termination\n");
		exit(1);
	}
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		fprintf(stderr, "Error with Socket Connection: %s\n", host);
		fprintf(stderr, "Restart Client. Program Termination\n");

		close(s);
		exit(1);
	}

	/* main loop: get and send lines of text */
	message_t msg;
	time_t time_elapsed = 0;
	time_t start_time = time(NULL);
	while (1) {
		switch(state) {
		// We should know whether we're in INTRO_ACK or INTRO_WAIT before we come here. 
			case INTRO_ACK:
				// Send ACK.
				msg = create_message(ACK_MSG, name, name, seq_num++);
				send(s, &msg, sizeof(message_t), 0);
				state = CHAT;
				break;
			case INTRO_WAIT:
				msg = create_message(INTRODUCTION_MSG, name, me_host, seq_num++);
				send(s, &msg, sizeof(msg), 0);
				fprintf(stdout, "Waiting for ACK from server.\r\n");
				fflush(stdout);
				start_time = time(NULL);
				while (difftime(start_time, time(NULL)) < TIMEOUT_SECONDS) {
					ssize_t n;
					if ((n = read(fd, rx_buf, sizeof(rx_buf))) > 0) {
						msg = get_message_from_buffer(rx_buf);
						if (msg.msg_type == ACK_MSG) {
							//fprintf(stdout, "Received ACK from %s.\r\n", msg.name);
							//fflush(stdout);
							state = CHAT;
							break;
						}
					}
				}
				if (state != CHAT){
					fprintf(stdout, "Timeout waiting for Introduction from server.\r\n");
					fflush(stdout);
					state = INTRO_WAIT;
				}
				break;
			case CHAT:
				ssize_t n;
				int i = poll(&pfds, 1, 0);
				if (i == -1) {
					perror("poll");
					exit(EXIT_FAILURE);
				}
				revents = pfds.revents;
				if (revents & POLLIN) {
					if ((n = read(fd, rx_buf, sizeof(rx_buf))) > 0) {
						msg = get_message_from_buffer(rx_buf);
						switch (msg.msg_type) {
							case CHAT_MSG:
								state = CHAT_ACK;
								break;
							case PROBE_MSG:
								state = PROBE_ACK;
								break;
							case NAME_CHANGE_MSG:
								state = CHANGE_NAME_ACK;
								break;
							case GOODBYE_MSG:
								state = GOODBYE_ACK;
								break;
							case INTRODUCTION_MSG:
								state = INTRO_ACK;
								break;
							case ACK_MSG:
								state = CHAT;
								break;
							default:
								fprintf(stdout, "unknown message recieved \n");
								fflush(stdout);
						}
					}
				}
				if (revents & POLLHUP) {
					close(pfds.fd);
				}
				if(fgets(tx_buf, sizeof(tx_buf), stdin)){
					//tx_buf[MAX_LINE-1] = '\0';
					//len = strlen(tx_buf) + 1;
					//send(s, tx_buf, len, 0);
					tx_buf[strcspn(tx_buf, "\n")] = 0;
					//fprintf(stdout, "MSG: %s.\n", tx_buf);
					//fflush(stdout);
					if(strstr(tx_buf,":p") != NULL){
						state = PROBE_WAIT;
						msg = create_message(PROBE_MSG, name, "", seq_num++);
						send(s, &msg, sizeof(msg), 0);
						break;
					}else if(strstr(tx_buf,":nc") != NULL){
						state = CHANGE_NAME_WAIT;
						char buf[MAX_NAME_LEN];
						strncpy(buf, strstr(tx_buf,":nc") + 4, sizeof(MAX_NAME_LEN));
						msg = create_message(NAME_CHANGE_MSG, name, buf, seq_num++);
						strncpy(name, buf, sizeof(name));
						send(s, &msg, sizeof(msg), 0);
						break;
					}else if(strstr(tx_buf, ":q") != NULL){
						state = GOODBYE_WAIT;
						msg = create_message(GOODBYE_MSG, name, "", seq_num++);
						send(s, &msg, sizeof(msg), 0);
						break;
					}
				}
				msg = create_message(CHAT_MSG, name, tx_buf, seq_num++);
				send(s, &msg, sizeof(msg), 0);
				state = CHAT_WAIT;
				break;
			case CHAT_WAIT:
				start_time = time(NULL);
				while (difftime(start_time, time(NULL)) < TIMEOUT_SECONDS) {
					ssize_t n;
					if ((n = read(fd, rx_buf, sizeof(rx_buf))) > 0) {
						msg = get_message_from_buffer(rx_buf);
						if (msg.msg_type == ACK_MSG) {
							//fprintf(stdout, "Received ACK from %s.\r\n", msg.name);
							//fflush(stdout);
							state = CHAT;
							break;
						}
					}
				}
				if (state != CHAT){
					fprintf(stdout, "Timeout waiting for Chat ACK from server.\r\n");
					fflush(stdout);
					state = CHAT_WAIT;
				}
				break;
			case CHAT_ACK:
				char buf[20];
				snprintf(buf, sizeof(buf), "%d", msg.sequence_number);
				msg = create_message(ACK_MSG, name, buf, seq_num++);
				send(s, &msg, sizeof(message_t), 0);
				state = CHAT;
				break;
			case PROBE_ACK:
				msg = create_message(ACK_MSG, name, "", seq_num++);
				send(s, &msg, sizeof(message_t), 0);
				state = CHAT;
				break;
			case PROBE_WAIT:
				start_time = time(NULL);
				while (time_elapsed < TIMEOUT_SECONDS) {
					time_elapsed = difftime(start_time, time(NULL));
					ssize_t n;
					if ((n = read(fd, rx_buf, sizeof(rx_buf))) > 0) {
						msg = get_message_from_buffer(rx_buf);
						if (msg.msg_type == ACK_MSG) {
							//fprintf(stdout, "Received Probe ACK from %s.\r\n", msg.name);
							fprintf(stdout, "Time Elapsed %d\r\n", time_elapsed);
							fflush(stdout);
							state = CHAT;
							break;
						}
					}
				}
				if (state != CHAT){
					fprintf(stdout, "Timeout waiting for Probe ACK from server.\r\n");
					fflush(stdout);
					state = PROBE_WAIT;
				}
				break;
			case CHANGE_NAME_ACK:
				msg = create_message(ACK_MSG, name, "", seq_num++);
				send(s, &msg, sizeof(message_t), 0);
				state = CHAT;
				break;
			case CHANGE_NAME_WAIT:
				start_time = time(NULL);
				while (difftime(start_time, time(NULL)) < TIMEOUT_SECONDS) {
					ssize_t n;
					if ((n = read(fd, rx_buf, sizeof(rx_buf))) > 0) {
						msg = get_message_from_buffer(rx_buf);
						if (msg.msg_type == ACK_MSG) {
							//fprintf(stdout, "Received ACK from %s.\r\n", msg.name);
							//fflush(stdout);
							state = CHAT;
							break;
						}
					}
				}
				if (state != CHAT){
					fprintf(stdout, "Timeout waiting for Name Change ACK from server.\r\n");
					fflush(stdout);
					state = CHANGE_NAME_WAIT;
				}
				break;
			case GOODBYE_WAIT:
				// Send the message and now wait for (hopefully) and ACK but doesn't really matter if we get it or not because we're going to die anyway.
				fprintf(stdout, "Waiting for Goodbye ACK from server.\r\n");
				fflush(stdout);
				start_time = time(NULL);
				while (difftime(start_time, time(NULL)) < TIMEOUT_SECONDS){
					ssize_t n;
					if ((n = read(fd, rx_buf, sizeof(rx_buf))) > 0) {
						msg = get_message_from_buffer(rx_buf);
						if (msg.msg_type == ACK_MSG) {
							//fprintf(stdout, "Received Goodbye ACK from %s.\r\n", msg.name);
							//fflush(stdout);
							break;
						}
					}
				}
				fprintf(stdout, "Timeout waiting for Goodbye ACK from server.\r\n");
				fflush(stdout);
				// Doesn't matter because we die regardless. :((
				break;
			case GOODBYE_ACK: 
				msg = create_message(ACK_MSG, name, "", seq_num++);
				send(s, &msg, sizeof(message_t), 0);
				break;
			default:
				fprintf(stdout, "Unknown state, going back to chat");
				fflush(stdout);
				state = CHAT;
		}
		if (state == GOODBYE_WAIT || state == GOODBYE_ACK) break;
	}
	pthread_join(thread, NULL);
	close(fd);
	fprintf(stdout, "End of line.");
	return 0;
}