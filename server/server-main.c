#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

void signal_handler(int);

int32_t ServerExit = 0;
int main(int argc, char *argv[]) {
	int portnum = 25566;
	if(argc >= 2) {
		portnum = atoi(argv[1]);
		if(portnum == 0) {
			portnum = 25566;
			printf("Bad port number passed, defaulting to 25566\n");
		}
	}
	signal(SIGINT, signal_handler);
	//signal(SIGIO, signal_handler);
	int ssock = socket(AF_INET, SOCK_STREAM, 0);
	if(ssock < 0) {
		perror("socket() errored ");
		return 1;
	}
	if(fcntl(ssock, F_SETFD, O_ASYNC) < 0) {
		perror("fcntl() failed ");
		close(ssock);
		return 1;
	}
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(portnum);
	if(bind(ssock, (struct sockaddr*)&addr, sizeof(addr) ) < 0) {
		perror("bind() errored ");
		close(ssock);
		return 1;
	}
	if(listen(ssock, 3) < 0) {
		perror("listen() failed ");
		close(ssock);
		return 1;
	}
	printf("Server opened on port %d\n", portnum);
	while(ServerExit == 0) {
		int r = accept(ssock, NULL, NULL);
		if(r < 0) {
			perror("accept() failed. ");
			break;
		}
		if(fcntl(r, F_SETFD, O_ASYNC) < 0) {
			perror("fcntl() failed. ");
			break;
		}
	}
	printf("Good bye.\n");
	close(ssock);
}

void signal_handler(int sigtype) {
	if(sigtype == SIGINT) { ServerExit = 1; }
}
