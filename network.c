/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

network.c: process network packets
*/

#include "main.h"

extern smpstatus_t SMPStatus;
size_t RXSMPEventLen; //Remote event len
size_t TXSMPEventLen; //Client Event len
uint8_t RXSMPEventBuffer[SMP_EVENT_BUFFER_SIZE], TXSMPEventBuffer[SMP_EVENT_BUFFER_SIZE]; //SMP Event Buffer
uint8_t SMPsalt[SALT_LENGTH]; //Filled when connected
int32_t SMPcid; //My client id
extern int32_t SMPProfCount;
extern SMPProfile_t *SMPProfs;
extern int32_t SelectedSMPProf;
extern SMPPlayers_t SMPPlayerInfo[MAX_CLIENTS];

//Packet receiver handler
void net_recv_handler() {
	//BUG: 2 send sonetimes integrated to one recv event
	//This will break packet interchange mechanism.
	uint8_t recvdata[NET_RX_BUFFER_SIZE];
	ssize_t r;
	r = recv_tcp_socket(recvdata, NET_RX_BUFFER_SIZE);
	if(r == 0) {
		printf("net_recv_handler(): Disconnect detected.\n");
		chat_request( (char*)getlocalizedstring(TEXT_DISCONNECTED) );
		close_connection_silent();
		return;
	}
	if(r < 0) {
		printf("net_recv_handler(): Socket recv() error detected.\n");
		chatf_request("%s %s", getlocalizedstring(TEXT_SMP_ERROR), strerror(errno) );
		close_connection_silent();
		return;
	}
	//Handle packet
	if(recvdata[0] == NP_RESP_DISCONNECT) {
		//Disconnect packet
		char reason[NET_RX_BUFFER_SIZE];
		memcpy(reason, &recvdata[1], (size_t)r - 1);
		reason[r - 1] = 0;
		printf("net_recv_handler(): Disconnect request: %s\n", reason);
		//Request to show chat message 
		chatf_request("%s %s", getlocalizedstring(TEXT_DISCONNECTED), reason);
		close_connection_silent();
	} else if(recvdata[0] == NP_GREETINGS) {
		//greetings response
		//Security update: avoid sending credentials for multiple times
		if(SMPcid != -1) {
			printf("net_recv_handler(): Duplicate greetings packet.\n");
			chat_request( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
			close_connection_silent();
			return;
		}
		//Data length check
		if(r != sizeof(np_greeter_t) ) {
			printf("net_recv_handler(): Too short greetings packet.\n");
			chat_request( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
			close_connection_silent();
			return;
		}
		np_greeter_t *p = (np_greeter_t*)recvdata;
		SMPcid = (int32_t)p->cid;
		memcpy(SMPsalt, p->salt, SALT_LENGTH);
		printf("net_recv_handler(): Server greeter received. CID=%d.\n", SMPcid);
		//printf("net_recv_handler(): Salt: ");
		//for(int32_t i = 0; i < SALT_LENGTH; i++) {
		//	printf("%02x", p->salt[i]);
		//}
		printf("\n");
		net_server_send_cmd(NP_LOGIN_WITH_PASSWORD); //Send credentials
	} else if(recvdata[0] == NP_LOGIN_WITH_PASSWORD) {
		//Login response: returns current userlist
		int32_t p = 1;
		int32_t c = 0;
		while(p < r && c < MAX_CLIENTS) {
			//Convert header
			userlist_hdr_t *uhdr = (userlist_hdr_t*)&recvdata[p];
			int32_t csiz = sizeof(userlist_hdr_t) + uhdr->uname_len;
			//Length check
			if(p + csiz > r) {
				printf("net_recv_handler(): Bad login response\n");
				chat_request( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
				close_connection_silent();
				return;
			}
			//Fill client information
			SMPPlayerInfo[c].cid = uhdr->cid;
			SMPPlayerInfo[c].pid = 0;
			SMPPlayerInfo[c].respawn_timer = -1;
			memcpy(SMPPlayerInfo[c].usr, &recvdata[p + (int32_t)sizeof(userlist_hdr_t)], uhdr->uname_len);
			c++;
			p += csiz;
		}
		if(c >= MAX_CLIENTS) {
			printf("net_recv_handler(): Bad login response\n");
			chat_request( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
			close_connection_silent();
			return;
		}
		SMPStatus = NETWORK_LOGGEDIN;
		printf("net_recv_handler(): Login successful response.\n");
	} else if(recvdata[0] == NP_EXCHANGE_EVENTS) {
		//Exchange event response
		if(r >= SMP_EVENT_BUFFER_SIZE) {
			printf("net_recv_handler(): GetEvent: Buffer overflow.\n");
			close_connection_silent();
			chat_request( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
		}
		memcpy(RXSMPEventBuffer, &recvdata[1], (size_t)r - 1);
		RXSMPEventLen = (size_t)r - 1;
	} else {
		close_connection_silent();
		chat_request( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
		printf("net_recv_handler(): Unknown Packet (%d): ", r);
		for(int32_t i = 0; i < r; i++) {
			printf("%02x ", recvdata[i]);
		}
		printf("\n");
	}
}

//Connect to specified address
void connect_server() {
	if(!is_range(SelectedSMPProf, 0, SMPProfCount - 1) ) {
		printf("connect_server(): Bad profile number.\n", SMPProfCount);
		return;
	}
	//If already connected, return
	if(SMPStatus != NETWORK_DISCONNECTED) {
		chat( (char*)getlocalizedstring(TEXT_UNAVAILABLE) ); //Unavailable
		printf("connect_server(): Already connected.\n");
		return;
	}
	//chatf(); //Attempting to connect
	showstatus("%s %s", getlocalizedstring(17), SMPProfs[SelectedSMPProf].host);
	printf("connect_server(): Attempting to connect to %s:%s\n", SMPProfs[SelectedSMPProf].host, SMPProfs[SelectedSMPProf].port);
	RXSMPEventLen = 0;
	TXSMPEventLen = 0;
	SMPcid = -1;
	for(int32_t i = 0; i < MAX_CLIENTS; i++) {
		SMPPlayerInfo[i].cid = -1;
	}
	SMPStatus = NETWORK_CONNECTING;
	//Connect (TODO: this will block main thread, use another thread instead.)
	if(make_tcp_socket(SMPProfs[SelectedSMPProf].host, SMPProfs[SelectedSMPProf].port) != 0) {
		chat( (char*)getlocalizedstring(18) ); //can not connect
		SMPStatus = NETWORK_DISCONNECTED;
		return;
	}
	chat( (char*)getlocalizedstring(20) ); //connected
	SMPStatus = NETWORK_CONNECTED;
}

//Close connection with reason (Is it working as expected?)
void close_connection(int32_t reason) {
	if(SMPStatus != NETWORK_DISCONNECTED) {
		if(reason == 1) {
			chatf("%s %s", getlocalizedstring(TEXT_SMP_ERROR), strerror(errno) );
		} else {
			chat( (char*)getlocalizedstring(19) );
		}
		close_connection_silent();
	} else {
		chat( (char*)getlocalizedstring(10) );
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
void net_server_send_cmd(server_command_t cmd) {
	uint8_t cmdbuf[NET_TX_BUFFER_SIZE];
	size_t ctxlen = 0;
	//Combine with command
	cmdbuf[0] = (uint8_t)cmd;
	switch(cmd) {
	case NP_EXCHANGE_EVENTS:
		if(TXSMPEventLen + 1 >= NET_TX_BUFFER_SIZE) {
			printf("net_server_send_cmd(): ADD_EVENT: Packet buffer overflow.\n");
			close_connection_silent();
			chat_request( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
			return;
		}
		memcpy(&cmdbuf[1], TXSMPEventBuffer, TXSMPEventLen);
		ctxlen = TXSMPEventLen;
		TXSMPEventLen = 0;
		break;
	case NP_LOGIN_WITH_PASSWORD:
		if(!is_range(SelectedSMPProf, 0, SMPProfCount) ) {
			printf("net_server_send_command(): NP_LOGIN_WITH_PASSWORD: Bad SMPSelectedProf number!!\n");
			return;
		}
		printf("net_server_send_cmd(): NP_LOGIN_WITH_PASSWORD: Attempeting to login to SMP server as %s\n", SMPProfs[SelectedSMPProf].usr);
		//Make login key (SHA512 of username+password+salt)
		GChecksum *keygen;
		gsize sp = NET_TX_BUFFER_SIZE - 1;
		keygen = g_checksum_new(G_CHECKSUM_SHA512);
		g_checksum_update(keygen, SMPProfs[SelectedSMPProf].usr, (gssize)strlen(SMPProfs[SelectedSMPProf].usr) );
		g_checksum_update(keygen, SMPProfs[SelectedSMPProf].pwd, (gssize)strlen(SMPProfs[SelectedSMPProf].pwd) );
		g_checksum_update(keygen, SMPsalt, SALT_LENGTH );
		g_checksum_get_digest(keygen, &cmdbuf[1], &sp);
		g_checksum_free(keygen);
		if(sp >= NET_TX_BUFFER_SIZE - 1) {
			printf("np_server_send_cmd(): NP_LOGIN_WITH_PASSWORD: SHA512 failed. Buffer overflow.\n");
			return;
		}
		ctxlen = sp;
	break;
	default:
		printf("net_server_send_command(): Bad command.\n");
		return;
	}
	if(send_tcp_socket(cmdbuf, ctxlen + 1) != 0) {
		close_connection(1);
	}
}

