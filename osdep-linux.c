/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

osdep-linux.c: os dependent codes for linux
*/

#include "main.h"

#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>

void netio_handler(int);

int ConnectionSocket = -1;

//Open and connect tcp socket to hostname and port
int32_t make_tcp_socket(char* hostname, char* port) {
	//If already connected, this function will fail.
	if(ConnectionSocket != -1) {
		printf("make_tcp_socket(): already connected.\n");
		return -1;
	}
	//Name Resolution
	struct addrinfo *addr, hint;
	memset(&hint, 0, sizeof(hint) );
	hint.ai_flags = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	int32_t r = getaddrinfo(hostname, port, &hint, &addr);
	if(r != 0) {
		//getaddrinfo error
		printf("make_tcp_socket(): getaddrinfo failed. (%d)\n", r);
		return -1;
	}
	//Make socket
	ConnectionSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(ConnectionSocket < 0) {
		//Socket error
		printf("make_tcp_socket(): socket creation failed. (%d)\n", ConnectionSocket);
		freeaddrinfo(addr);
		return -1;
	}
	//Connect to node
	if(connect(ConnectionSocket, addr->ai_addr, addr->ai_addrlen) != 0) {
		printf("make_tcp_socket(): connect failed. %s.\n", strerror(errno) );
		close(ConnectionSocket);
		ConnectionSocket = -1;
	}
	freeaddrinfo(addr);
	if(ConnectionSocket == -1) {
		return -1;
	}
	//Make socket async, set handler
	if(fcntl(ConnectionSocket, F_SETFL, O_ASYNC) == -1 || fcntl(ConnectionSocket, F_SETOWN, getpid() ) == -1) {
		printf("make_tcp_socket(): setting socket option failed.\n");
		close(ConnectionSocket);
		ConnectionSocket = -1;
	}
	return 0;
}

//Close current connection
int32_t close_tcp_socket() {
	if(ConnectionSocket == -1) {
		printf("close_tcp_socket(): socket is not open!\n");
		return -1;
	}
	close(ConnectionSocket);
	ConnectionSocket = -1;
	return 0;
}

//Send bytes to connected server
int32_t send_tcp_socket(uint8_t* ctx, size_t ctxlen) {
	if(ConnectionSocket == -1) {
		printf("send_tcp_socket(): socket is not open!\n");
		return -1;
	}
	if(send(ConnectionSocket, ctx, ctxlen, MSG_NOSIGNAL) != ctxlen) {
		printf("send_tcp_socket(): send failed. Closing current connection.\n");
		close_tcp_socket();
		return -1;
	}
	return 0;
}

void netio_handler(int) { //left for compatibility
	//net_recv_handler();
}

//Install IO Handler, Returns 0 if succeed (linux uses network callback: left for comaptibility)
int32_t install_io_handler() {
	//struct sigaction sig;
	//memset(&sig, 0, sizeof(struct sigaction) );
	//sig.sa_handler = netio_handler;
	//return sigaction(SIGIO, &sig, NULL);
	return -1;
}

//Receive bytes from connected server, returns read bytes
ssize_t recv_tcp_socket(uint8_t* ctx, size_t ctxlen) {
	if(ConnectionSocket == -1) {
		printf("recv_tcp_socket(): socket is not open!\n");
		return -1;
	}
	return recv(ConnectionSocket, ctx, ctxlen, 0);
}

void poll_tcp_socket() {
	//Do nothing in linux, linux code does not use poll strategy
}
