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

#include "inc/zundagame.h"

#include <string.h>
#include <stdarg.h>
#include <errno.h>

void process_smp_events(uint8_t*, size_t, int32_t);
void net_server_send_cmd(server_command_t);
void pkt_recv_handler(uint8_t*, size_t);
void connection_establish_handler();
void connection_close_handler();

extern int32_t CurrentPlayableCharacterID;
extern GameObjs_t Gobjs[MAX_OBJECT_COUNT];
extern smpstatus_t SMPStatus;
size_t RXSMPEventLen; //Remote event len
size_t TXSMPEventLen; //Client Event len
uint8_t RXSMPEventBuffer[NET_BUFFER_SIZE], TXSMPEventBuffer[NET_BUFFER_SIZE]; //SMP Event Buffer
uint8_t SMPsalt[SALT_LENGTH]; //Filled when connected
int32_t SMPcid; //My client id
extern int32_t SMPProfCount;
extern SMPProfile_t *SMPProfs;
extern int32_t SelectedSMPProf;
extern SMPPlayers_t SMPPlayerInfo[MAX_CLIENTS];
uint8_t TempRXBuffer[NET_BUFFER_SIZE];
size_t TRXBLength;
extern int32_t NetworkTimeout;
int32_t TimeoutTimer;
int32_t ConnectionTimeoutTimer;
extern int32_t DifEnemyBaseCount[4];
extern int32_t DifEnemyBaseDist;
extern double DifATKGain;
int32_t DisconnectReasonProvided = 0;

#ifdef __WASM
	extern void wasm_login_to_smp(char*, char*, uint8_t*);
#endif

void close_connection_cmd() {
	info("Disconnect requested from user.\n");
	close_connection(NULL);
}

//Called every 10 ms, do network task. not called in wasm ver.
void network_recv_task() {
	//If not connected, return
	if(SMPStatus == NETWORK_DISCONNECTED) {
		return;
	}

	//If connect() is in progress
		if(SMPStatus == NETWORK_CONNECTING) {

		//Process connection timeout (5Sec)
		if(ConnectionTimeoutTimer > 500) {
			warn("network_recv_task(): Connection timed out.\n");
			close_connection((char*)getlocalizedstring(TEXT_SMP_TIMEOUT) );
			return;
		} else {
			ConnectionTimeoutTimer++;
		}
				
		//Check if connected
		if(isconnected_tcp_socket() == 1) {
			connection_establish_handler();
		}
		return;
	}

	//Receive into buffer
	uint8_t b[NET_BUFFER_SIZE];
	ssize_t r = recv_tcp_socket(b, NET_BUFFER_SIZE);
	if(r == 0) { //If recv returns 0, disconnected
		warn("network_recv_task(): disconnected from server.\n");
		close_connection(NULL);
		return;
	} else if(r == -1) { //no data
		//EWOULDBLOCK or EAGAIN ... no data
		//Check for timeout
		TimeoutTimer++;
		if(NetworkTimeout != 0 && TimeoutTimer >= NetworkTimeout) {
			warn("network_recv_task(): Timed out, no packet longer than timeout.\n");
			close_connection((char*)getlocalizedstring(TEXT_SMP_TIMEOUT) );
		}
		return;
	} else if(r == -2) { //recv error
		warn("network_recv_task(): recv failed\n");
		close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR) );
		return;
	}
	TimeoutTimer = 0;

	//Process data
	//Length 0-FD    XX
	//Length FE-FFFF FE XX XX
	//concat TempRXBuffer and b into tmpbuf
	uint8_t tmpbuf[NET_BUFFER_SIZE * 2];
	memcpy(tmpbuf, TempRXBuffer, TRXBLength);
	memcpy(&tmpbuf[TRXBLength], b, (size_t)r);
	size_t totallen = TRXBLength + (size_t)r;
	//Process for each packet
	size_t p = 0;
	while(p < totallen) {
		size_t remain = totallen - p;
		size_t reqsize, lenhdrsize;
		//Get packet data length
		if(is_range(tmpbuf[p], 0, 0xfd) ) {
			//first packet byte 0x0 - 0xfd means the byte represents length as uint8_t
			lenhdrsize = 1;
			reqsize = tmpbuf[p] + lenhdrsize;
		} else if(tmpbuf[p] == 0xfe) {
			//first packet byte 0xfe means next two bytes represents length as uint16_t
			lenhdrsize = 3;
			if(remain >= lenhdrsize) {
				uint16_t *_v = (uint16_t*)&tmpbuf[p + 1];
				reqsize = (size_t)(network2host_fconv_16(*_v) + 3);
			} else {
				break; //Not enough packet received
			}
		}
		//Check buffer overflow
		if(reqsize > NET_BUFFER_SIZE) {
			warn("network_recv_task(): data length exceeds buffer size. closing connection.\n");
			close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR));
			return;
		}
		if(remain < reqsize) {
			break; //Not enough packet received
		} else {
			pkt_recv_handler(&tmpbuf[p + lenhdrsize], reqsize - lenhdrsize);
		}
		p += reqsize;
	}
	
	//Push unprocessed data into TempRXBuffer
	size_t remain = totallen - p;
	memcpy(TempRXBuffer, &tmpbuf[p], remain);
	TRXBLength = remain;
}

void connection_close_handler() {
	if(SMPStatus != NETWORK_DISCONNECTED) {
		info("connection_close_handler() called.\n");
		close_connection(NULL);
	}
}

void connection_establish_handler() {
	showstatus( (char*)getlocalizedstring(20) ); //connected
	info("Connected to remote SMP server.\n");
	SMPStatus = NETWORK_CONNECTED;
	#ifndef __WASM
	send_tcp_socket("\n", 1);
	#endif
}

//Packet receiver handler
void pkt_recv_handler(uint8_t *pkt, size_t plen) {
	//Handle packet
	if(pkt[0] == NP_RESP_DISCONNECT) {
		//Disconnect packet
		char reason[NET_BUFFER_SIZE];
		memcpy(reason, &pkt[1], plen - 1);
		reason[plen - 1] = 0;
		info("pkt_recv_handler(): Disconnect request: %s\n", reason);
		close_connection(reason);
	} else if(pkt[0] == NP_GREETINGS) {
		//greetings response
		//Security update: avoid sending credentials for multiple times
		if(SMPcid != -1) {
			warn("pkt_recv_handler(): Duplicate greetings packet.\n");
			close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR));
			return;
		}
		//Data length check
		if(plen != sizeof(np_greeter_t) ) {
			warn("pkt_recv_handler(): Too short greetings packet.\n");
			close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR));
			return;
		}
		np_greeter_t *p = (np_greeter_t*)pkt;
		SMPcid = (int32_t)p->cid;
		memcpy(SMPsalt, p->salt, SALT_LENGTH);
		info("pkt_recv_handler(): Server greeter received. CID=%d.\n", SMPcid);
		//Send credentials
		net_server_send_cmd(NP_LOGIN_WITH_HASH);
	} else if(pkt[0] == NP_RESP_LOGON) {
		//Login response: returns current userlist
		size_t p = 1;
		int32_t c = 0;
		while(p < plen && c < MAX_CLIENTS) {
			//Convert header
			userlist_hdr_t *uhdr = (userlist_hdr_t*)&pkt[p];
			size_t csiz = sizeof(userlist_hdr_t) + (size_t)(uhdr->uname_len);
			//Length check
			if(p + csiz > plen) {
				warn("pkt_recv_handler(): Bad login response\n");
				close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR));
				return;
			}
			//Fill client information
			SMPPlayerInfo[c].cid = uhdr->cid;
			SMPPlayerInfo[c].pid = 0;
			SMPPlayerInfo[c].respawn_timer = -1;
			memcpy(SMPPlayerInfo[c].usr, &pkt[p + (int32_t)sizeof(userlist_hdr_t)], uhdr->uname_len);
			c++;
			p += csiz;
		}
		if(c >= MAX_CLIENTS) {
			warn("pkt_recv_handler(): Bad login response\n");
			close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR));
			return;
		}
		SMPStatus = NETWORK_LOGGEDIN;
		info("pkt_recv_handler(): Login successful response.\n");
	} else if(pkt[0] == NP_EXCHANGE_EVENTS) {
		//Exchange event response
		if(plen >= NET_BUFFER_SIZE) {
			warn("pkt_recv_handler(): GetEvent: Buffer overflow.\n");
			close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR));
		}
		memcpy(RXSMPEventBuffer, &pkt[1], plen - 1);
		RXSMPEventLen = plen - 1;
		//printf("pkt_recv_handler(): GetEvent: RXSMPEventlen: %d\n", RXSMPEventLen);
	} else {
		close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR));
		warn("pkt_recv_handler(): Unknown Packet (%ld): ", plen);
		for(size_t i = 0; i < plen; i++) {
			warn("%02x ", pkt[i]);
		}
		warn("\n");
	}
}

//Connect to specified address
void connect_server() {
	//Check for parameter
	if(!is_range(SelectedSMPProf, 0, SMPProfCount - 1) ) {
		warn("connect_server(): Bad profile number: %d\n", SMPProfCount);
		showstatus( (char*)getlocalizedstring(TEXT_UNAVAILABLE) ); //Unavailable
		return;
	}

	//If already connected, return
	if(SMPStatus != NETWORK_DISCONNECTED) {
		showstatus( (char*)getlocalizedstring(TEXT_UNAVAILABLE) ); //Unavailable
		warn("connect_server(): Already connected.\n");
		return;
	}

	//Announce and reset variables
	showstatus("%s %s", getlocalizedstring(17), SMPProfs[SelectedSMPProf].host);
	info("connect_server(): Attempting to connect to %s:%s\n", SMPProfs[SelectedSMPProf].host, SMPProfs[SelectedSMPProf].port);
	RXSMPEventLen = 0;
	TXSMPEventLen = 0;
	TRXBLength = 0;
	SMPcid = -1;
	TimeoutTimer = 0;
	ConnectionTimeoutTimer = 0;
	DisconnectReasonProvided = 0;
	for(int32_t i = 0; i < MAX_CLIENTS; i++) {
		SMPPlayerInfo[i].cid = -1;
	}
	SMPStatus = NETWORK_CONNECTING;

	//Connect
	if(make_tcp_socket(SMPProfs[SelectedSMPProf].host, SMPProfs[SelectedSMPProf].port) != 0) {
		showstatus( (char*)getlocalizedstring(18) ); //can not connect
		SMPStatus = NETWORK_DISCONNECTED;
		return;
	}
}

//Close connection but do not announce.
void close_connection(char* reason) {
	if(SMPStatus != NETWORK_DISCONNECTED) {
		if(DisconnectReasonProvided == 0) {
			if(reason != NULL) {
				showstatus("%s %s", (char*)getlocalizedstring(TEXT_DISCONNECTED), reason);
			} else {
				showstatus((char*)getlocalizedstring(TEXT_DISCONNECTED));
			}
			DisconnectReasonProvided = 1;
		}
		close_tcp_socket();
		SMPStatus = NETWORK_DISCONNECTED;
	}
}

//Send Command to server
void net_server_send_cmd(server_command_t cmd) {
	uint8_t cmdbuf[NET_BUFFER_SIZE];
	size_t ctxlen = 0;
	//Combine with command
	cmdbuf[0] = (uint8_t)cmd;
	if(cmd == NP_EXCHANGE_EVENTS) {
		//NP_EXCHANGE_EVENT: put buffered event to server and get remote events
		if(TXSMPEventLen + 1 >= NET_BUFFER_SIZE) {
			warn("net_server_send_cmd(): ADD_EVENT: Packet buffer overflow.\n");
			close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR));
			return;
		}
		memcpy(&cmdbuf[1], TXSMPEventBuffer, TXSMPEventLen);
		ctxlen = TXSMPEventLen + 1;
		TXSMPEventLen = 0;
	} else if(cmd == NP_LOGIN_WITH_HASH) {
		//NP_LOGIN_WITH_HASH: login with sha512 of username + password + salt
		if(!is_range(SelectedSMPProf, 0, SMPProfCount - 1) ) {
			warn("net_server_send_command(): NP_LOGIN_WITH_HASH: Bad SMPSelectedProf number!!\n");
			return;
		}
		char *usr = SMPProfs[SelectedSMPProf].usr;
		info("net_server_send_cmd(): NP_LOGIN_WITH_HASH: Attempeting to login to SMP server as %s\n", usr);
		char *pwd = SMPProfs[SelectedSMPProf].pwd;
		//Make login key (SHA512 of username+password+salt)
		#ifdef __WASM
			//In WASM version, browser will make login key and send it
			wasm_login_to_smp(usr, pwd, SMPsalt);
			return;
		#endif
		uint8_t ph[SHA512_LENGTH];
		if(compute_passhash(usr, pwd, SMPsalt, ph) != 0)  {
			warn("np_server_send_cmd(): NP_LOGIN_WITH_HASH: calculating hash failed.\n");
			return;
		}
		memcpy(&cmdbuf[1], ph, SHA512_LENGTH); //combine command and hash
		ctxlen = SHA512_LENGTH + 1;
	} else {
		warn("net_server_send_command(): Bad command.\n");
		return;
	}
	#ifdef __WASM
		//WASM version should send message without size header, websocket will care for it
		send_tcp_socket(cmdbuf, ctxlen);
	#else
		//Check for size and make size header, assemble packet.
		size_t hdrlen;
		uint8_t buf[NET_BUFFER_SIZE];
		if(is_range( (int32_t)ctxlen, 0, 0xfd) ) {
			buf[0] = (uint8_t)ctxlen;
			hdrlen = 1;
		} else if(is_range( (int32_t)ctxlen, 0xfe, 0xffff) ) {
			uint16_t *p = (uint16_t*)&cmdbuf[1];
			buf[0] = 0xfe;
			*p = (uint16_t)network2host_fconv_16( (uint16_t)ctxlen);
			hdrlen = 3;
		} else {
			warn("net_server_send_cmd(): incompatible packet size, not sending packet.\n");
			return;
		}
		size_t totallen = ctxlen + hdrlen;
		if(totallen > NET_BUFFER_SIZE) {
			warn("net_server_send_command(): buffer overflow, not sending packet.\n");
			return;
		}
		memcpy(&buf[hdrlen], cmdbuf, ctxlen);

		//Send
		if(send_tcp_socket(buf, totallen) != totallen) {
			close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR));
		}
	#endif
}

//Process SMP packet
void process_smp() {
	//Process respawn timer
	for(int32_t i = 0; i < MAX_CLIENTS; i++) {
		if(SMPPlayerInfo[i].cid != -1 && SMPPlayerInfo[i].respawn_timer > 0) {
			SMPPlayerInfo[i].respawn_timer--;
			//If timer went to 0, respawn playable character of SMP client
			if(SMPPlayerInfo[i].respawn_timer == 0) {
				int32_t r = spawn_playable(SMPPlayerInfo[i].pid);
				SMPPlayerInfo[i].playable_objid = r;
			}
		}
	}
	//Process event from remote
	event_hdr_t *hdr;
	if(RXSMPEventLen > 0) {
		//Process SMP Event
		uint32_t ptr = 0;
		while(ptr < RXSMPEventLen) {
			size_t rem = RXSMPEventLen - ptr;
			//Parse event record header
			if(rem < sizeof(event_hdr_t) ) {
				warn("process_smp(): header read failed, rem:%ld, ptr: %ld\n", rem, ptr);
				close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR));
				return;
			}
			hdr = (event_hdr_t*)&RXSMPEventBuffer[ptr];
			int32_t cid = hdr->cid;
			size_t evlen = (size_t)network2host_fconv_16(hdr->evlen);
			size_t reclen = evlen + sizeof(event_hdr_t);
			uint8_t* evhead = &RXSMPEventBuffer[ptr + (int32_t)sizeof(event_hdr_t)];
			if(rem < reclen) {
				warn("process_smp(): Too short event record. rem: %ld, ptr: %ld\n", rem, ptr);
				close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR));
				return;
			}
			process_smp_events(evhead, evlen, cid);
			ptr += (uint32_t)reclen;
		}
		RXSMPEventLen = 0;
	}
	net_server_send_cmd(NP_EXCHANGE_EVENTS);
}

//Stack packet into event buffer for future sending
void stack_packet(event_type_t etype, ...) {
	va_list varg;
	size_t pktlen;
	uint8_t *poff = &TXSMPEventBuffer[TXSMPEventLen];
	va_start(varg, etype);
	//Assemble event packet and stock
	if(etype == EV_PLAYABLE_LOCATION) {	
		//Playable coordinate notify event
		int16_t tx = (uint16_t)va_arg(varg, double);
		int16_t ty = (uint16_t)va_arg(varg, double);
		ev_playablelocation_t ev = {
			.evtype = (uint8_t)etype,
			.x = host2network_fconv_16(tx),
			.y = host2network_fconv_16(ty)
		};
		pktlen = sizeof(ev_playablelocation_t);
		if(TXSMPEventLen + pktlen < NET_BUFFER_SIZE) {
			memcpy(poff, &ev, pktlen);
		}
	} else if(etype == EV_PLACE_ITEM) {
		//item place event
		uint8_t tid = (uint8_t)va_arg(varg, int);
		int16_t x = (int16_t)va_arg(varg, double);
		int16_t y = (int16_t)va_arg(varg, double);
		ev_placeitem_t ev = {
			.evtype = (uint8_t)etype,
			.tid = tid,
			.x = host2network_fconv_16(x),
			.y = host2network_fconv_16(y)
		};
		pktlen = sizeof(ev_placeitem_t);
		if(TXSMPEventLen + pktlen < NET_BUFFER_SIZE) {
			memcpy(poff, &ev, pktlen);
		}
	} else if(etype == EV_USE_SKILL) {
		//skill using event
		uint8_t skillid = (uint8_t)va_arg(varg, int);
		ev_useskill_t ev = {
			.evtype = (uint8_t)etype,
			.skillid = skillid
		};
		pktlen = sizeof(ev_useskill_t);
		if(TXSMPEventLen + pktlen < NET_BUFFER_SIZE) {
			memcpy(poff, &ev, pktlen);
		}
	} else if(etype == EV_CHAT) {
		//chat event
		char* ctx = (char*)va_arg(varg, char*);
		size_t chatplen = sizeof(ctx);
		ev_chat_t ev = {
			.evtype = etype,
			.dstcid = 0,
			.clen = host2network_fconv_16( (int16_t)chatplen)
		};
		if(is_range( (int32_t)chatplen, 0, NET_CHAT_LIMIT - 1) ) {
			pktlen = sizeof(ev_chat_t) + chatplen;
		} else {
			warn("net_stack_packet(): CHAT: too long message\n");
			pktlen = 0;
		}
		if(TXSMPEventLen + pktlen < NET_BUFFER_SIZE) {
			memcpy(poff, &ev, sizeof(ev_chat_t) );
			memcpy(&poff[sizeof(ev_chat_t)], ctx, chatplen);
		}
	} else if(etype == EV_RESET) {
		//Game round reset event
		ev_reset_t ev = {
			.evtype = etype,
			.level_seed = host2network_fconv_32(rand() ),
			.basedistance = host2network_fconv_16( (int16_t)DifEnemyBaseDist),
			.basecount0 = DifEnemyBaseCount[0],
			.basecount1 = DifEnemyBaseCount[1],
			.basecount2 = DifEnemyBaseCount[2],
			.basecount3 = DifEnemyBaseCount[3],
			.atkgain = (float)DifATKGain
		};
		pktlen = sizeof(ev_reset_t);
		if(TXSMPEventLen + pktlen < NET_BUFFER_SIZE) {
			memcpy(poff, &ev, sizeof(ev_reset_t) );
		}
	} else if(etype == EV_CHANGE_PLAYABLE_ID) {
		//playable character id change event
		ev_changeplayableid_t ev = {
			.evtype = etype,
			.pid = (uint8_t)CurrentPlayableCharacterID
		};
		pktlen = sizeof(ev_changeplayableid_t);
		if(TXSMPEventLen + pktlen < NET_BUFFER_SIZE) {
			memcpy(poff, &ev, sizeof(ev_changeplayableid_t) );
		}
	} else if(etype == EV_CHANGE_ATKGAIN) {
		//atkgain change event
		ev_changeatkgain_t ev = {
			.evtype = etype,
			.atkgain = (float)DifATKGain
		};
		pktlen = sizeof(ev_changeatkgain_t);
		if(TXSMPEventLen + pktlen < NET_BUFFER_SIZE) {
			memcpy(poff, &ev, pktlen);
		}
	} else {
		warn("net_stack_packet(): unknown packet type.\n");
		pktlen = 0;
	}
	va_end(varg);
	//Test Packet
	//const uint8_t TESTPACKET[] = "\x03\xff\x00\x05Hello";
	//if(TXSMPEventLen + pktlen + sizeof(TESTPACKET) < NET_BUFFER_SIZE) {
	//	memcpy(poff + pktlen, TESTPACKET, sizeof(TESTPACKET) );
	//	pktlen += sizeof(TESTPACKET);
	//} else {
	//	warn("Injecting test packet failed!\n");
	//}
	if(TXSMPEventLen + pktlen >= NET_BUFFER_SIZE) {
		warn("net_stack_packet(): TX SMP buffer overflow.\n");
		close_connection((char*)getlocalizedstring(TEXT_SMP_ERROR));
	} else {
		TXSMPEventLen += pktlen;
	}
}

//Process smp events (evbuf: content, evlen: length, cid: event owner id)
void process_smp_events(uint8_t* evbuf, size_t evlen, int32_t cid) {
	//Decode packet
	size_t evp = 0;
	while(evp < evlen) {
		ssize_t remaining = (ssize_t)evlen - (ssize_t)evp;
		if(evbuf[evp] == EV_PLAYABLE_LOCATION) {
			if(remaining < sizeof(ev_playablelocation_t) ) {
				warn("process_smp_events(): Too short EV_PLAYABLE_LOCATION packet, decoder will terminate.\n");
				return;
			}
			ev_playablelocation_t *ev = (ev_playablelocation_t*)&evbuf[evp];
			double tx = (double)network2host_fconv_16(ev->x);
			double ty = (double)network2host_fconv_16(ev->y);
			//Security check
			if(is_range_number(tx, 0, MAP_WIDTH) && is_range_number(ty, 0, MAP_HEIGHT)) {
				int32_t i = lookup_smp_player_from_cid(cid);
				if(is_range(i, 0, MAX_CLIENTS - 1) ) {
					int32_t p_objid = SMPPlayerInfo[i].playable_objid;
					if(is_range(p_objid, 0, MAX_OBJECT_COUNT - 1) ) {
						Gobjs[p_objid].x = tx;
						Gobjs[p_objid].y = ty;
					} else {
						warn("process_smp_events(): EV_PLAYABLE_LOCATION: Character of CID%d is not generated yet.\n", cid);
					}
				} else {
					//Not registed in client list.
					warn("process_smp_events(): EV_PLAYABLE_LOCATION: CID%d is not registered yet.\n", cid);
				}
			} else {
				warn("process_smp_events(): CHANGE_PLAYABLE_LOCATION: packet check failed.\n");
			}
			evp += sizeof(ev_playablelocation_t);
		} else if(evbuf[evp] == EV_PLACE_ITEM) {
			if(remaining < sizeof(ev_placeitem_t) ) {
				warn("process_smp_events(): Too short EV_PLACE_ITEM packet, decoder will terminate.\n");
				return;
			}
			//ItemPlace
			ev_placeitem_t *ev = (ev_placeitem_t*)&evbuf[evp];
			obj_type_t tid = (obj_type_t)ev->tid;
			double x = (double)network2host_fconv_16(ev->x);
			double y = (double)network2host_fconv_16(ev->y);
			//Security check
			if(is_range_number(x, 0, MAP_WIDTH) && is_range_number(y, 0, MAP_HEIGHT) && is_range(tid, 0, MAX_TID - 1) ) {
				//Place object and set client id
				int32_t i = lookup_smp_player_from_cid(cid);
				if(is_range(i, 0, MAX_CLIENTS - 1) ) {
					int32_t p_objid = SMPPlayerInfo[i].playable_objid;
					if(is_range(p_objid, 0, MAX_OBJECT_COUNT - 1) ) {
						info("process_smp_events(): PLACE_ITEM: cid=%d, tid=%d, x=%f, y=%f\n", cid, tid, x, y);
						add_character(tid, x, y, i);
					} else {
						warn("process_smp_events(): PLACE_ITEM ignored, CID%d does not have playable character.\n", cid);
					}
				} else {
					warn("process_smp_events(): CID%d is not in client list yet!\n", cid);
				}
			} else {
				warn("process_smp_events(): PLACE_ITEM: packet check failed.\n");
			}
			evp += sizeof(ev_placeitem_t);
		} else if(evbuf[evp] == EV_USE_SKILL) {
			if(remaining < sizeof(ev_useskill_t) ) {
				warn("process_smp_events(): Too short EV_USE_SKILL packet, decoder will terminate.\n");
				return;
			}
			//UseSkill
			ev_useskill_t *ev = (ev_useskill_t*)&evbuf[evp];
			uint8_t skillid = ev->skillid;
			//Security check
			if(is_range(skillid, 0, SKILL_COUNT - 1) ) {
				int32_t i = lookup_smp_player_from_cid(cid);
				if(is_range(i, 0, MAX_CLIENTS - 1) ) {
					int32_t pid = SMPPlayerInfo[i].playable_objid;
					if(is_range(pid, 0, MAX_OBJECT_COUNT - 1) ) {
						PlayableInfo_t p;
						lookup_playable(pid, &p);
						use_skill(i, skillid, p);
					} else {
						//g_print("process_smp_events(): USESKILL: User cid%d has invalid playable_objid.\n", cid);
					}
				} else {
					warn("process_smp_events(): USESKILL: cid%d is not in client list yet.\n", cid);
				}
			} else {
				warn("process_smp_events(): USESKILL: packet check failed.\n");
			}
			evp += sizeof(ev_useskill_t);
		} else if(evbuf[evp] == EV_CHAT) {
			if(remaining < sizeof(ev_chat_t) ) {
				warn("process_smp_events(): Too short EV_CHAT packet, decoder will terminate.\n");
				return;
			}
			//Chat on other client
			ev_chat_t *ev = (ev_chat_t*)&evbuf[evp];
			char ctx[BUFFER_SIZE];
			ssize_t cpktlen = (ssize_t)network2host_fconv_16(ev->clen);
			char* netchat = (char*)&evbuf[evp + (int32_t)sizeof(ev_chat_t)];
			if((ssize_t)remaining < (ssize_t)sizeof(ev_chat_t) + cpktlen) {
				warn("process_smp_events(): Too short EV_CHAT packet, decoder will terminate.\n");
				return;
			}
			//Security check
			if(is_range( (int32_t)cpktlen, 0, NET_CHAT_LIMIT - 1) ) {
				memcpy(ctx, netchat, (size_t)cpktlen);
				ctx[cpktlen] = 0; //Terminate string with NUL
				int32_t cidinf = lookup_smp_player_from_cid(cid);
				if(cidinf == -1) {
					chatf("[SMP-CID%d] %s", cid, ctx);
				} else {
					chatf("<%s> %s", SMPPlayerInfo[cid].usr, ctx);
				}
			} else {
				warn("procss_smp_events(): CHAT: packet too large.\n");
			}
			evp += sizeof(ev_chat_t) + cpktlen;
		} else if(evbuf[evp] == EV_RESET) {
			//Reset request
			if(remaining < sizeof(ev_reset_t) ) {
				warn("process_smp_events(): Too short EV_RESET packet, decoder will terminate.\n");
				return;
			}
			ev_reset_t *ev = (ev_reset_t*)&evbuf[evp];
			uint32_t _seed = (uint32_t)network2host_fconv_32(ev->level_seed);
			DifEnemyBaseDist = (int32_t)network2host_fconv_16(ev->basedistance);
			DifATKGain = ev->atkgain;
			DifEnemyBaseCount[0] = ev->basecount0;
			DifEnemyBaseCount[1] = ev->basecount1;
			DifEnemyBaseCount[2] = ev->basecount2;
			DifEnemyBaseCount[3] = ev->basecount3;
			srand(_seed);
			info("process_smp_events(): Round reset request from cid%d (Seed=%d)\n", cid, _seed);
			reset_game();
			evp += sizeof(ev_reset_t);
		} else if(evbuf[evp] == EV_HELLO) {
			//Another client connect event
			if(remaining < sizeof(ev_hello_t) ) {
				warn("process_smp_events(): Too short EV_HELLO packet, decoder will terminate.\n");
				return;
			}
			ev_hello_t *ev = (ev_hello_t*)&evbuf[evp];
			int32_t fcid = (int32_t)ev->cid;
			size_t funame_len = (size_t)ev->uname_len;
			if(remaining < (size_t)sizeof(ev_hello_t) + funame_len) {
				warn("process_smp_events(): Too short EV_HELLO packet, decoder will terminate.\n");
				return;
			}
			char funame[UNAME_SIZE];
			if(funame_len >= UNAME_SIZE) {
				warn("process_smp_events(): EV_HELLO packet username too large.\n");
				return;
			}
			memcpy(funame, &evbuf[evp + (int32_t)sizeof(ev_hello_t)], funame_len);
			funame[funame_len] = 0;
			if(cid == -1) {
				info("process_smp_events(): Another client joined: %s(%d).\n", funame, fcid);
				chatf("%s %s", funame, getlocalizedstring(22) );
				//Register client info
				for(int32_t i = 0; i < MAX_CLIENTS; i++) {
					if(SMPPlayerInfo[i].cid == -1 || SMPPlayerInfo[i].cid == fcid) {
						SMPPlayerInfo[i].cid = fcid;
						SMPPlayerInfo[i].pid = 0;
						SMPPlayerInfo[i].playable_objid = OBJID_INVALID;
						strcpy(SMPPlayerInfo[i].usr, funame);
						break;
					}
				}
			} else if(evbuf[evp] == EV_CHANGE_ATKGAIN) {
				//Change AtkGain Parameter request
				if(remaining < sizeof(ev_changeatkgain_t) ) {
					warn("process_smp_events(): Too short EV_CHANGE_ATKGAIN packet, decoder will terminate.\n");
					return;
				}
				ev_changeatkgain_t *ev = (ev_changeatkgain_t*)&evbuf[evp];
				if(is_range_number(ev->atkgain, MIN_ATKGAIN, MAX_ATKGAIN) ) {
					DifATKGain = ev->atkgain;
				} else {
					warn("process_smp_events(): EV_CHANGE_ATKGAIN wrong parameter!");
				}
			} else {
				warn("process_smp_events(): EV_JOIN: security check failed.\n");
			}
			evp += sizeof(ev_hello_t) + funame_len;
			} else if(evbuf[evp] == EV_BYE) {
			//Another client disconnect event
			if(remaining < sizeof(ev_bye_t) ) {
				warn("process_smp_events(): Too short EV_BYE packet, decoder will terminate.\n");
				return;
			}
			ev_bye_t *ev = (ev_bye_t*)&evbuf[evp];
			int32_t fcid = (int32_t)ev->cid;
			//This packet should issued from cid -1 (server)
			if(cid == -1) {
				info("process_smp_events(): Another client disconnected: %d.\n", fcid);
				//Clean up left user's playable character
				int32_t i = lookup_smp_player_from_cid(fcid);
				if(is_range(i, 0, MAX_CLIENTS) ) {
					chatf("%s %s", SMPPlayerInfo[i].usr, getlocalizedstring(23) );
					int32_t j = SMPPlayerInfo[i].playable_objid;
					info("Trying to clear playable object id=%d\n", j);
					if(is_range(j, 0, MAX_OBJECT_COUNT - 1) ) {
						Gobjs[j].tid = TID_NULL;
					} else {
						warn("process_smp_events(): CID%d does not have playable character.\n", fcid);
					}
					SMPPlayerInfo[i].cid = -1;
				} else {
					warn("process_smp_events(): CID%d is not on client list.\n", cid);
				}
			} else {
				warn("process_smp_events(): EV_BYE: security check failed.\n");
			}
			evp += sizeof(ev_bye_t);
		} else if(evbuf[evp] == EV_CHANGE_PLAYABLE_ID) {
			//Another client playable id change
			if(remaining < sizeof(ev_changeplayableid_t) ) {
				warn("process_smp_events(): Too short EV_CHANGE_PLAYABLE_ID packet, decoder will terminate.\n");
				return;
			}
			ev_changeplayableid_t *ev = (ev_changeplayableid_t*)&evbuf[evp];
			//Param check
			if(is_range(ev->pid, 0, PLAYABLE_CHARACTERS_COUNT - 1) ) {
				info("process_smp_events(): Another client(%d) playable id changed to %d\n", cid, ev->pid);
				int32_t i = lookup_smp_player_from_cid(cid);
				if(is_range(i, 0, MAX_CLIENTS) ) {
					SMPPlayerInfo[i].pid = ev->pid;
				} else {
					warn("process_smp_events(): EV_CHANGE_PLAYABLE_ID: cid %d is not registed.\n", cid);
				}
			} else {
				warn("process_smp_events(): EV_CHANGE_PLAYABLE_ID: parameter check failed.\n");
			}
			evp += sizeof(ev_changeplayableid_t);
		} else {
			warn("process_smp_events(): Unknown event type, decorder terminated.\n");
			break;
		}
	}
}

int32_t lookup_smp_player_from_cid(int32_t cid) {
	for(int32_t i = 0; i < MAX_CLIENTS; i++) {
		if(SMPPlayerInfo[i].cid == cid) {
			return i;
		}
	}
	return -1;
}
