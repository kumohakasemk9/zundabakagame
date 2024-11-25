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

#ifndef WIN32
	#include <arpa/inet.h>
#else
	#include <winsock.h>
#endif

extern char SMPUsername[UNAME_SIZE], SMPPassword[PASSWD_SIZE];
extern smpstatus_t SMPStatus;
ssize_t RXSMPEventLen, TXSMPEventLen; //Current Event Sizes
uint8_t RXSMPEventBuffer[SMP_EVENT_BUFFER_SIZE], TXSMPEventBuffer[SMP_EVENT_BUFFER_SIZE]; //SMP Event Buffer
int32_t SMPsalt; //Filled when connected
int32_t SMPcid; //My client id

//Packet receiver handler
void net_recv_handler() {
	//BUG: 2 send sonetimes integrated to one recv event
	//This will break packet interchange mechanism.
	uint8_t recvdata[NET_RX_BUFFER_SIZE];
	int32_t r;
	r = recv_tcp_socket(recvdata, NET_RX_BUFFER_SIZE);
	if(r == 0) {
		g_print("net_recv_handler(): Disconnect detected.\n");
		chat_request(getlocalizedstring(TEXT_DISCONNECTED) );
		close_connection_silent();
		return;
	}
	if(r < 0) {
		g_print("net_recv_handler(): Socket recv() error detected.\n");
		chatf_request("%s %s", getlocalizedstring(TEXT_SMP_ERROR), strerror(errno) );
		close_connection_silent();
		return;
	}
	//Handle packet
	if(recvdata[0] == NP_RESP_DISCONNECT) {
		//Disconnect packet
		char reason[NET_RX_BUFFER_SIZE];
		memcpy(reason, &recvdata[1], r);
		reason[r] = 0;
		g_print("net_recv_handler(): Disconnect request: %s\n", reason);
		//Request to show chat message 
		chatf_request("%s %s", getlocalizedstring(TEXT_DISCONNECTED), reason);
		close_connection_silent();
	} else if(recvdata[0] == NP_GREETINGS) {
		//greetings response
		//Data length check
		if(r != sizeof(np_greeter_t) ) {
			g_print("net_recv_handler(): Bat greetings packet.\n");
			chat_request(getlocalizedstring(TEXT_SMP_ERROR) );
			close_connection_silent();
			return;
		}
		np_greeter_t *p = (np_greeter_t*)recvdata;
		SMPcid = p->cid;
		SMPsalt = ntohl(p->salt);
		g_print("net_recv_handler(): Server greeter received. CID=%d Salt=%d\n", SMPcid, SMPsalt);
		net_server_send_cmd(NP_LOGIN_WITH_PASSWORD); //Send credentials
	} else if(recvdata[0] == NP_LOGIN_WITH_PASSWORD) {
		if(r == 2 && recvdata[1] == '+') {
			//Login command successful response
			SMPStatus = NETWORK_LOGGEDIN;
			g_print("net_recv_handler(): Login successful response.\n");
		} else {
			g_print("net_recv_handler(): Bad login response\n");
			chat_request(getlocalizedstring(TEXT_SMP_ERROR) );
			close_connection_silent();
			return;

		}
	} else if(recvdata[0] == NP_EXCHANGE_EVENTS) {
		//Exchange event response
		if(r >= SMP_EVENT_BUFFER_SIZE) {
			g_print("net_recv_handler(): GetEvent: Buffer overflow.\n");
			close_connection_silent();
			chat_request(getlocalizedstring(TEXT_SMP_ERROR) );
		}
		memcpy(RXSMPEventBuffer, &recvdata[1], r - 1);
		RXSMPEventLen = r - 1;
	} else {
		close_connection_silent();
		chat_request(getlocalizedstring(TEXT_SMP_ERROR) );
		g_print("net_recv_handler(): Unknown Packet (%d): ", r);
		for(int32_t i = 0; i < r; i++) {
			g_print("%02x ", recvdata[i]);
		}
		g_print("\n");
	}
}

//Connect to specified address
void connect_server(char* addrstr) {
	//If credential not set, return
	if(strlen(SMPUsername) == 0 || strlen(SMPPassword) == 0) {
		chat(getlocalizedstring(TEXT_UNAVAILABLE) ); //Unavailable
		g_print("connect_server(): Empty credentials.\n");
		return;
	}
	//If already connected, return
	if(SMPStatus != NETWORK_DISCONNECTED) {
		chat(getlocalizedstring(TEXT_UNAVAILABLE) ); //Unavailable
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
	g_print("connect_server(): Attempting to connect to host=%s port=%s\n", s_host, s_port);
	RXSMPEventLen = 0;
	TXSMPEventLen = 0;
	SMPStatus = NETWORK_CONNECTING;
	//Connect (TODO: this will block main thread, use another thread instead.)
	if(make_tcp_socket(s_host, s_port) != 0) {
		chat(getlocalizedstring(18) ); //can not connect
		SMPStatus = NETWORK_DISCONNECTED;
		return;
	}
	chat(getlocalizedstring(20) ); //connected
	SMPStatus = NETWORK_CONNECTED;
}

//Close connection with reason (Is it working as expected?)
void close_connection(int32_t reason) {
	if(SMPStatus != NETWORK_DISCONNECTED) {
		if(reason == 1) {
			chatf("%s %s", getlocalizedstring(TEXT_SMP_ERROR), strerror(errno) );
		} else {
			chat(getlocalizedstring(19) );
		}
		close_connection_silent();
	} else {
		chat(getlocalizedstring(10) );
	}
}

//Close connection but do not announce.
void close_connection_silent() {
	if(SMPStatus != NETWORK_DISCONNECTED) {
		close_tcp_socket();
		SMPStatus = NETWORK_DISCONNECTED;
	}
}

//Send Command to server
void net_server_send_cmd(server_command_t cmd, ...) {
	uint8_t cmdbuf[NET_TX_BUFFER_SIZE];
	size_t ctxlen = 0;
	//Combine with command
	cmdbuf[0] = (uint8_t)cmd;
	switch(cmd) {
	case NP_EXCHANGE_EVENTS:
		if(TXSMPEventLen + 1 >= NET_TX_BUFFER_SIZE) {
			g_print("net_server_send_cmd(): ADD_EVENT: Packet buffer overflow.\n");
			close_connection_silent();
			chat_request(getlocalizedstring(TEXT_SMP_ERROR) );
			return;
		}
		memcpy(&cmdbuf[1], TXSMPEventBuffer, TXSMPEventLen);
		ctxlen = TXSMPEventLen;
		TXSMPEventLen = 0;
		break;
	case NP_LOGIN_WITH_PASSWORD:
		strncpy(&cmdbuf[1], SMPUsername, NET_TX_BUFFER_SIZE - 1);
		strncat(cmdbuf, ":", NET_TX_BUFFER_SIZE - 1);
		strncat(cmdbuf, SMPPassword, NET_TX_BUFFER_SIZE - 1);
		ctxlen = strlen(cmdbuf) - 1;
	break;
	default:
		g_print("net_server_send_command(): Bad command.\n");
		return;
	}
	if(send_tcp_socket(cmdbuf, ctxlen + 1) != 0) {
		close_connection(1);
	}
}
