/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by Gtk4

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

osdep.c: os dependent codes
*/

#include "main.h"

#ifndef WIN32
//linux codes

#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>

int ConnectionSocket = -1;

//Open and connect tcp socket to hostname and port
int32_t make_tcp_socket(char* hostname, char* port) {
	//If already connected, this function will fail.
	if(is_open_tcp_socket() == 0) {
		g_print("make_tcp_socket(): already connected.\n");
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
		g_print("make_tcp_socket(): getaddrinfo failed. (%d)\n", r);
		return -1;
	}
	//Make socket
	ConnectionSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(ConnectionSocket < 0) {
		//Socket error
		g_print("make_tcp_socket(): socket creation failed. (%d)\n", ConnectionSocket);
		freeaddrinfo(addr);
		return -1;
	}
	//Connect to node
	if(connect(ConnectionSocket, addr->ai_addr, addr->ai_addrlen) != 0) {
		g_print("make_tcp_socket(): connect failed. %s.\n", strerror(errno) );
		close(ConnectionSocket);
		ConnectionSocket = -1;
	}
	freeaddrinfo(addr);
	if(ConnectionSocket == -1) {
		return -1;
	}
	return 0;
}

//Close current connection
int32_t close_tcp_socket() {
	if(is_open_tcp_socket() == -1) {
		g_print("close_tcp_socket(): socket is not open!\n");
		return -1;
	}
	close(ConnectionSocket);
	ConnectionSocket = -1;
	return 0;
}

//Send bytes to connected server
int32_t send_tcp_socket(uint8_t* ctx, size_t ctxlen) {
	if(is_open_tcp_socket() == -1) {
		g_print("send_tcp_socket(): socket is not open!\n");
		return -1;
	}
	if(send(ConnectionSocket, ctx, ctxlen, 0) != ctxlen) {
		g_print("send_tcp_socket(): send failed. Closing current connection.\n");
		close_tcp_socket();
		return -1;
	}
	return 0;
}

//Returns 0 if connected, -1 if closed
int32_t is_open_tcp_socket() {
	if(ConnectionSocket < 0) { return -1; }
	return 0;
}

#else
//Windows codes
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

SOCKET ConnectionSocket = INVALID_SOCKET;

int32_t make_tcp_socket(char *hostname, char* port) {
	if(is_open_tcp_socket() == 0) {
		g_print("make_tcp_socket() win32: Already connected\n");
		return -1;
	}
	WSADATA wsaData;
	int iResult;
	//init winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if(iResult != 0) {
		g_print("make_tcp_socket() win32: WSAStartup failed: %d\n", iResult);
		return -1;
	}
	//convert given hostname and port into addrinfo
	struct addrinfo *ret = NULL, hint;
	ZeroMemory(&hint, sizeof(hint) );
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = IPPROTO_TCP;
	iResult = getaddrinfo(hostname, port, &hint, &ret);
	if(iResult != 0) {
		g_print("make_tcp_socket() win32: getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return -1;
	}
	//Create socket
	ConnectionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(ConnectionSocket == INVALID_SOCKET) {
		g_print("make_tcp_socket() win32: Socket creation error. %ld\n", WSAGetLastError() );
		freeaddrinfo(ret);
		WSACleanup();
		return -1;
	}
	//Connect
	iResult = connect(ConnectionSocket, ret->ai_addr, (int)ret->ai_addrlen);
	if(iResult == SOCKET_ERROR) {
		g_print("make_tcp_socket() win32: Unable to connect server. %ld\n", WSAGetLastError() );
		closesocket(ConnectionSocket);
		ConnectionSocket = INVALID_SOCKET;
	}
	freeaddrinfo(ret);
	if(ConnectionSocket == INVALID_SOCKET) {
		WSACleanup();
		return -1;
	}
	return 0;
}

int32_t close_tcp_socket() {
	if(is_open_tcp_socket() == INVALID_SOCKET) {
		g_print("close_tcp_socket() win32: Socket is not open.\n");
		return -1;
	}
	closesocket(ConnectionSocket);
	ConnectionSocket = INVALID_SOCKET;
	WSACleanup();
	return 0;
}

int32_t send_tcp_socket(uint8_t *ctx, size_t ctxlen) {
	if(is_open_tcp_socket() == INVALID_SOCKET) {
		g_print("send_tcp_socket() win32: Socket is not open.\n");
		return -1;
	}
	if(send(ConnectionSocket, ctx, ctxlen, 0) != ctxlen) {
		g_print("send_tcp_socket() win32: Unable to send data. Closing tcp socket.\n");
		close_tcp_socket();
		return -1;
	}
}

int32_t is_open_tcp_socket() {
	if(ConnectionSocket == INVALID_SOCKET) { return -1; }
	return 0;
} 

#endif
