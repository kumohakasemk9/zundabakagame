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

//Send packet to other clients, parameters change according to pkttype.
void net_send_packet(networkpackettype_t pkttype, ...) {
	uint8_t pktbuf[1024];
	size_t pktlen = 0;
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
	//Send packet if socket connected
	if(is_open_tcp_socket() == 0) {
		send_tcp_socket(pktbuf, pktlen);
	}
}

//Connect to specified address
void connect_server(char* addrstr) {
	//If already connected, return
	if(is_open_tcp_socket() == 0) {
		chat(getlocalizedstring(10) ); //Unavailable
		g_print("connect_server(): Already connected.\n");
		return;
	}
	//If addrstr has port number (2nd arg), divide addrstr into host and port then 
	//override default port
	char s_port[10] = "25566";
	char s_host[64];
	size_t siz_hostname;
	char *t = g_utf8_strchr(addrstr, g_utf8_strlen(addrstr, 65535), 0x20); //find whitespace
	if(t != NULL) {
		//hostname port
		strncpy(s_port, t + 1, sizeof(s_port) );
		s_port[sizeof(s_port) - 1] = 0; //Additional Security
		siz_hostname = constrain_i32(t - addrstr, 0, sizeof(s_host) - 1);
	} else {
		siz_hostname = g_utf8_strlen(addrstr, sizeof(s_host) - 1);
	}
	memcpy(s_host, addrstr, siz_hostname);
	s_host[siz_hostname] = 0;
	chatf("%s %s", getlocalizedstring(17), addrstr); //Attempting to connect
	g_print("Attempting to connect to host=%s port=%s\n", s_host, s_port);
	//Connect
	if(make_tcp_socket(s_host, s_port) != 0) {
		chat(getlocalizedstring(18) );
		return;
	}
	chat(getlocalizedstring(20) );
}

//Close connection with reason
void close_connection(int32_t reason) {
	if(is_open_tcp_socket() == 0) {
		close_tcp_socket();
		chat(getlocalizedstring(19) );
	} else {
		chat(getlocalizedstring(10) );
	}
}
