/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by Gtk4

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

network.c: process network packets
*/

#include "main.h"

extern int NetworkSocket;

//Send packet to other clients, parameters change according to pkttype.
void net_send_packet(networkpackettype_t pkttype, ...) {
	uint8_t pktbuf[1024];
	uint8_t pktlen = 0;
	va_list varg;
	//first packet byte always pkttype
	pktbuf[0] = (uint8_t)pkttype;
	va_start(varg, pkttype);
	//make packet
	switch(pkttype) {
	case NETPACKET_CHANGE_PLAYABLE_SPEED:
		double tx = (double)va_arg(varg, double);
		double ty = (double)va_arg(varg, double);
		double2bytes(tx, pktbuf, 1);
		double2bytes(ty, pktbuf, 9);
		pktlen = 17;
	break;
	case NETPACKET_PLACE_ITEM:
		int32_t tid = (int32_t)va_arg(varg, int);
		double x = (double)va_arg(varg, double);
		double y = (double)va_arg(varg, double);
		int322bytes(tid, pktbuf, 1);
		double2bytes(x, pktbuf, 5);
		double2bytes(y, pktbuf, 13);
		pktlen = 21;
	break;
	case NETPACKET_USE_SKILL:
		int32_t skillid = (int32_t)va_arg(varg, int);
		int322bytes(skillid, pktbuf, 1);
		pktlen = 5;
	break;
	}
	va_end(varg);
	//Send Packet
	if(pktlen != 0 && NetworkSocket != -1) {
		if(send(NetworkSocket, pktbuf, pktlen, 0) != pktlen) {
			//If error occured, close connection
			int32_t e = errno;
			g_print("Packet send error: %s\n", strerror(e) );
			close_connection(e);
		}
	}
}

//Connect to specified address
void connect_server(char* addrstr) {
	//Return if already connected
	if(NetworkSocket != -1) {
		chat(getlocalizedstring(10) ); //Unavailable
		//g_print("Already connected.\n");
		return;
	}
	chat(getlocalizedstring(17) ); //Attempting to connect
	//g_print("Attempting to connect to %s\n", addrstr);
	//Create socket
	NetworkSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(NetworkSocket < 0) {
		die("connect_server(): creating socket failed.\n");
		return;
	}
	//If there's :xxxxx, process it as a port name
	char *portnum = "25566"; //default port
	char *t = strchr(addrstr, ':');
	if(t != NULL) {
		*t = 0;
		portnum = t + 1;
	}
	//Hostname resolution and conversion
	struct addrinfo *res;
	if(getaddrinfo(addrstr, portnum, NULL, &res) != 0) {
		int t = errno;
		chatf("%s (%s)", getlocalizedstring(18), strerror(t) );
		//g_print("Name Resolution failed: %s\n", strerror(t) );
		close(NetworkSocket);
		NetworkSocket = -1;
		return;
	}
	//Connect
	if(connect(NetworkSocket, res->ai_addr, res->ai_addrlen) != 0) {
		int t = errno;
		chatf("%s (%s)", getlocalizedstring(18), strerror(t) );
		//g_print("Connect failed: %s\n", strerror(t) );
		close(NetworkSocket);
		NetworkSocket = -1;
		return;
	}
	freeaddrinfo(res); //free Name Resolution result
	chatf("%s (%s)", getlocalizedstring(20), addrstr);
	//g_print("Connection established: %s\n", addrstr);
}

//Close connection with reason
void close_connection(int32_t reason) {
	if(NetworkSocket == -1) {
		chat(getlocalizedstring(10) ); //Unavailable
		g_print("Not connected.\n");
		return;
	}
	close(NetworkSocket);
	NetworkSocket = -1;
	if(reason != -1) {
		chatf("%s (%s)", getlocalizedstring(19), strerror(reason) );
		g_print("Closed connection with reason : %s\n", strerror(reason) );
	} else {
		chat(getlocalizedstring(19) );
		g_print("Closed connection.\n");
	}
}
