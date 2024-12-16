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

extern int32_t CurrentPlayableCharacterID;
extern GameObjs_t Gobjs[MAX_OBJECT_COUNT];
extern int32_t Difficulty;
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
	if(cmd == NP_EXCHANGE_EVENTS) {
		if(TXSMPEventLen + 1 >= NET_TX_BUFFER_SIZE) {
			printf("net_server_send_cmd(): ADD_EVENT: Packet buffer overflow.\n");
			close_connection_silent();
			chat_request( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
			return;
		}
		memcpy(&cmdbuf[1], TXSMPEventBuffer, TXSMPEventLen);
		ctxlen = TXSMPEventLen;
		TXSMPEventLen = 0;
	} else if(cmd == NP_LOGIN_WITH_PASSWORD) {
		if(!is_range(SelectedSMPProf, 0, SMPProfCount - 1) ) {
			printf("net_server_send_command(): NP_LOGIN_WITH_PASSWORD: Bad SMPSelectedProf number!!\n");
			return;
		}
		printf("net_server_send_cmd(): NP_LOGIN_WITH_PASSWORD: Attempeting to login to SMP server as %s\n", SMPProfs[SelectedSMPProf].usr);
		//Make login key (SHA512 of username+password+salt)
		char *ph = compute_passhash(SMPProfs[SelectedSMPProf].usr, SMPProfs[SelectedSMPProf].pwd, SMPsalt);
		if(ph == NULL)  {
			printf("np_server_send_cmd(): NP_LOGIN_WITH_PASSWORD: calculating hash failed.\n");
			return;
		}
		size_t sp = strlen(ph);
		if(sp + 1 >= NET_TX_BUFFER_SIZE - 1) {
			printf("np_server_send_cmd(): NP_LOGIN_WITH_PASSWORD: Buffer overflow.\n");
			return;
		}
		memcpy(&cmdbuf[1], ph, sp); //combine command and hash
		ctxlen = sp;
	} else {
		printf("net_server_send_command(): Bad command.\n");
		return;
	}
	if(send_tcp_socket(cmdbuf, ctxlen + 1) != 0) {
		close_connection(1);
	}
}

//Process SMP packet
void process_smp() {
	poll_tcp_socket(); //For windows, poll network socket, linux uses callback.
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
				printf("process_smp(): Too short event record.\n");
				chat( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
				close_connection_silent();
				return;
			}
			hdr = (event_hdr_t*)&RXSMPEventBuffer[ptr];
			int32_t cid = hdr->cid;
			size_t evlen = g_ntohs(hdr->evlen);
			size_t reclen = evlen + sizeof(event_hdr_t);
			uint8_t* evhead = &RXSMPEventBuffer[ptr + (int32_t)sizeof(event_hdr_t)];
			if(rem < reclen) {
				printf("process_smp(): Too short event record.\n");
				chat( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
				close_connection_silent();
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
	if(etype == EV_CHANGE_PLAYABLE_SPEED) {	
		//Playable speed change event
		float tx = (float)va_arg(varg, double);
		float ty = (float)va_arg(varg, double);
		ev_changeplayablespeed_t ev = {
			.evtype = (uint8_t)etype,
			.sx = tx,
			.sy = ty
		};
		pktlen = sizeof(ev_changeplayablespeed_t);
		if(TXSMPEventLen + pktlen < SMP_EVENT_BUFFER_SIZE) {
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
		if(TXSMPEventLen + pktlen < SMP_EVENT_BUFFER_SIZE) {
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
		if(TXSMPEventLen + pktlen < SMP_EVENT_BUFFER_SIZE) {
			memcpy(poff, &ev, pktlen);
		}
	} else if(etype == EV_CHAT) {
		//chat event
		char* ctx = (char*)va_arg(varg, char*);
		size_t chatplen = sizeof(ctx);
		ev_chat_t ev = {
			.evtype = etype,
			.clen = host2network_fconv_16( (int16_t)chatplen)
		};
		if(is_range( (int32_t)chatplen, 0, NET_CHAT_LIMIT - 1) ) {
			pktlen = sizeof(ev_chat_t) + chatplen;
		} else {
			printf("net_stack_packet(): CHAT: too long message\n");
			pktlen = 0;
		}
		if(TXSMPEventLen + pktlen < SMP_EVENT_BUFFER_SIZE) {
			memcpy(poff, &ev, sizeof(ev_chat_t) );
			memcpy(&poff[sizeof(ev_chat_t)], ctx, chatplen);
		}
	} else if(etype == EV_RESET) {
		//Game round reset event
		ev_reset_t ev = {
			.evtype = etype,
			.level_seed = host2network_fconv_32(rand() ),
			.level_difficulty = host2network_fconv_32(Difficulty)
		};
		pktlen = sizeof(ev_reset_t);
		if(TXSMPEventLen + pktlen < SMP_EVENT_BUFFER_SIZE) {
			memcpy(poff, &ev, sizeof(ev_reset_t) );
		}
	} else if(etype == EV_CHANGE_PLAYABLE_ID) {
		//playable character id change event
		ev_changeplayableid_t ev = {
			.evtype = etype,
			.pid = (uint8_t)CurrentPlayableCharacterID
		};
		pktlen = sizeof(ev_changeplayableid_t);
		if(TXSMPEventLen + pktlen < SMP_EVENT_BUFFER_SIZE) {
			memcpy(poff, &ev, sizeof(ev_changeplayableid_t) );
		}
	} else {
		printf("net_stack_packet(): unknown packet type.\n");
		pktlen = 0;
	}
	va_end(varg);
	if(TXSMPEventLen + pktlen >= SMP_EVENT_BUFFER_SIZE) {
		printf("net_stack_packet(): TX SMP buffer overflow.\n");
		close_connection_silent();
		chat_request( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
	} else {
		TXSMPEventLen += pktlen;
	}
}

//Process smp events (evbuf: content, evlen: length, cid: event owner id)
void process_smp_events(uint8_t* evbuf, size_t evlen, int32_t cid) {
	//Decode packet
	int32_t evp = 0;
	while(evp < evlen) {
		ssize_t remaining = (ssize_t)evlen - (ssize_t)evp;
		if(evbuf[evp] == EV_CHANGE_PLAYABLE_SPEED) {
			if(remaining < sizeof(ev_changeplayablespeed_t) ) {
				printf("process_smp_events(): Too short EV_CHANGE_PLAYABLE_SPEED packet, decoder will terminate.\n");
				return;
			}
			ev_changeplayablespeed_t *ev = (ev_changeplayablespeed_t*)&evbuf[evp];
			double sx = (double)ev->sx;
			double sy = (double)ev->sy;
			//Security check
			if(is_range_number(sx, -10, 10) && is_range_number(sy, -10, 10)) {
				int32_t i = lookup_smp_player_from_cid(cid);
				if(is_range(i, 0, MAX_CLIENTS - 1) ) {
					int32_t p_objid = SMPPlayerInfo[i].playable_objid;
					if(is_range(p_objid, 0, MAX_OBJECT_COUNT - 1) ) {
						Gobjs[p_objid].sx = sx;
						Gobjs[p_objid].sy = sy;
					} else {
						//g_print("process_smp_events(): CHANGE_PALYABLE_SPEED: Character of CID%d is not generated yet.\n");
					}
				} else {
					//Not registed in client list.
					printf("process_smp_events(): CHANGE_PLAYABLE_SPEED: CID%d is not registered yet.\n", cid);
				}
			} else {
				printf("process_smp_events(): CHANGE_PLAYABLE_SPEED: packet check failed.\n");
			}
			evp += (int32_t)sizeof(ev_changeplayablespeed_t);
		} else if(evbuf[evp] == EV_PLACE_ITEM) {
			if(remaining < sizeof(ev_placeitem_t) ) {
				printf("process_smp_events(): Too short EV_PLACE_ITEM packet, decoder will terminate.\n");
				return;
			}
			//ItemPlace
			ev_placeitem_t *ev = (ev_placeitem_t*)&evbuf[evp];
			obj_type_t tid = (obj_type_t)ev->tid;
			double x = (double)network2host_fconv_32(ev->x);
			double y = (double)network2host_fconv_32(ev->y);
			//Security check
			if(is_range_number(x, 0, MAP_WIDTH) && is_range_number(y, 0, MAP_HEIGHT) && is_range(tid, 0, MAX_TID - 1) ) {
				//Place object and set client id
				int32_t i = lookup_smp_player_from_cid(cid);
				if(is_range(i, 0, MAX_CLIENTS - 1) ) {
					int32_t p_objid = SMPPlayerInfo[i].playable_objid;
					if(is_range(p_objid, 0, MAX_OBJECT_COUNT - 1) ) {
						printf("process_smp_events(): PLACE_ITEM: cid=%d, tid=%d, x=%f, y=%f\n", cid, tid, x, y);
						int32_t objid = add_character(tid, x, y, i);
					} else {
						//g_print("process_smp_events(): PLACE_ITEM ignored, CID%d does not have playable character.\n", cid);
					}
				} else {
					printf("process_smp_events(): CID%d is not in client list yet!\n", cid);
				}
			} else {
				printf("process_smp_events(): PLACE_ITEM: packet check failed.\n");
			}
			evp += (int32_t)sizeof(ev_placeitem_t);
		} else if(evbuf[evp] == EV_USE_SKILL) {
			if(remaining < sizeof(ev_useskill_t) ) {
				printf("process_smp_events(): Too short EV_USE_SKILL packet, decoder will terminate.\n");
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
					printf("process_smp_events(): USESKILL: cid%d is not in client list yet.\n", cid);
				}
			} else {
				printf("process_smp_events(): USESKILL: packet check failed.\n");
			}
			evp += (int32_t)sizeof(ev_useskill_t);
		} else if(evbuf[evp] == EV_CHAT) {
			if(remaining < sizeof(ev_chat_t) ) {
				printf("process_smp_events(): Too short EV_CHAT packet, decoder will terminate.\n");
				return;
			}
			//Chat on other client
			ev_chat_t *ev = (ev_chat_t*)&evbuf[evp];
			char ctx[BUFFER_SIZE];
			ssize_t cpktlen = (ssize_t)network2host_fconv_16(ev->clen);
			char* netchat = (char*)&evbuf[evp + (int32_t)sizeof(ev_chat_t)];
			if((ssize_t)remaining < (ssize_t)sizeof(ev_chat_t) + cpktlen) {
				printf("process_smp_events(): Too short EV_CHAT packet, decoder will terminate.\n");
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
				printf("procss_smp_events(): CHAT: packet too large.\n");
			}
			evp += (int32_t)sizeof(ev_chat_t) + (int32_t)cpktlen;
		} else if(evbuf[evp] == EV_RESET) {
			if(remaining < sizeof(ev_reset_t) ) {
				printf("process_smp_events(): Too short EV_RESET packet, decoder will terminate.\n");
				return;
			}
			ev_reset_t *ev = (ev_reset_t*)&evbuf[evp];
			int32_t _difficulty = network2host_fconv_32(ev->level_difficulty);
			uint32_t _seed = (uint32_t)network2host_fconv_32(ev->level_seed);
			srand(_seed);
			if(is_range(_difficulty, 1, MAX_DIFFICULTY) ) {
				Difficulty = _difficulty;
				printf("process_smp_events(): Round reset request from cid%d (Difficulty=%d, Seed=%d)\n", cid, Difficulty, _seed);
				reset_game();
			} else {
				printf("process_smp_events(): EV_RESET: packet check failure.\n");
			}
			evp += (int32_t)sizeof(ev_reset_t);
		} else if(evbuf[evp] == EV_HELLO) {
			//Another client connect event
			if(remaining < sizeof(ev_hello_t) ) {
				printf("process_smp_events(): Too short EV_HELLO packet, decoder will terminate.\n");
				return;
			}
			ev_hello_t *ev = (ev_hello_t*)&evbuf[evp];
			int32_t fcid = (int32_t)ev->cid;
			size_t funame_len = (size_t)ev->uname_len;
			if(remaining < (size_t)sizeof(ev_hello_t) + funame_len) {
				printf("process_smp_events(): Too short EV_HELLO packet, decoder will terminate.\n");
				return;
			}
			char funame[UNAME_SIZE];
			if(funame_len >= UNAME_SIZE) {
				printf("process_smp_events(): EV_HELLO packet username too large.\n");
				return;
			}
			memcpy(funame, &evbuf[evp + (int32_t)sizeof(ev_hello_t)], funame_len);
			funame[funame_len] = 0;
			if(cid == -1) {
				printf("process_smp_events(): Another client joined: %s(%d).\n", funame, fcid);
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
			} else {
				printf("process_smp_events(): EV_JOIN: security check failed.\n");
			}
			evp += (int32_t)sizeof(ev_hello_t) + (int32_t)funame_len;
			} else if(evbuf[evp] == EV_BYE) {
			//Another client disconnect event
			if(remaining < sizeof(ev_bye_t) ) {
				printf("process_smp_events(): Too short EV_BYE packet, decoder will terminate.\n");
				return;
			}
			ev_bye_t *ev = (ev_bye_t*)&evbuf[evp];
			int32_t fcid = (int32_t)ev->cid;
			//This packet should issued from cid -1 (server)
			if(cid == -1) {
				printf("process_smp_events(): Another client disconnected: %d.\n", fcid);
				//Clean up left user's playable character
				int32_t i = lookup_smp_player_from_cid(fcid);
				if(is_range(i, 0, MAX_CLIENTS) ) {
					chatf("%s %s", SMPPlayerInfo[i].usr, getlocalizedstring(23) );
					int32_t j = SMPPlayerInfo[i].playable_objid;
					if(is_range(0, j, MAX_OBJECT_COUNT - 1) ) {
						Gobjs[j].tid = TID_NULL;
					} else {
						printf("process_smp_events(): CID%d does not have playable character.\n", cid);
					}
					SMPPlayerInfo[i].cid = -1;
				} else {
					printf("process_smp_events(): CID%d is not on client list.\n", cid);
				}
			} else {
				printf("process_smp_events(): EV_BYE: security check failed.\n");
			}
			evp += (int32_t)sizeof(ev_bye_t);
		} else if(evbuf[evp] == EV_CHANGE_PLAYABLE_ID) {
			//Another client playable id change
			if(remaining < sizeof(ev_changeplayableid_t) ) {
				printf("process_smp_events(): Too short EV_CHANGE_PLAYABLE_ID packet, decoder will terminate.\n");
				return;
			}
			ev_changeplayableid_t *ev = (ev_changeplayableid_t*)&evbuf[evp];
			//Param check
			if(is_range(ev->pid, 0, PLAYABLE_CHARACTERS_COUNT - 1) ) {
				printf("process_smp_events(): Another client(%d) playable id changed to %d\n", cid, ev->pid);
				int32_t i = lookup_smp_player_from_cid(cid);
				if(is_range(i, 0, MAX_CLIENTS) ) {
					SMPPlayerInfo[i].pid = ev->pid;
				} else {
					printf("process_smp_events(): EV_CHANGE_PLAYABLE_ID: cid %d is not registed.\n", cid);
				}
			} else {
				printf("process_smp_events(): EV_CHANGE_PLAYABLE_ID: parameter check failed.\n");
			}
		} else {
			printf("process_smp_events(): Unknown event type, decorder terminated.\n");
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
