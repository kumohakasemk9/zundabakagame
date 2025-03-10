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

//Text consts
#define TEXT_DISCONNECTED 7 //Disconnected from server
#define TEXT_SMP_ERROR 9 //Disconnected because error in SMP routine
#define TEXT_SMP_TIMEOUT 12 //Timed out
#define TEXT_OFFLINE 13 //Offline
#define TEXT_UNAVAILABLE 2 //Command or item unavailable

#include "inc/zundagame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

extern int32_t EndlessMode;
extern int32_t PlayingCharacterID;
extern int32_t CurrentPlayableID;
extern GameObjs_t Gobjs[MAX_OBJECT_COUNT];
smpstatus_t SMPStatus = NETWORK_DISCONNECTED; //Current connection status
size_t RXSMPEventLen; //Remote event len
size_t TXSMPEventLen; //Client Event len
uint8_t RXSMPEventBuffer[NET_BUFFER_SIZE], TXSMPEventBuffer[NET_BUFFER_SIZE]; //SMP Event Buffer
uint8_t SMPsalt[SALT_LENGTH]; //Filled when connected
int32_t SMPcid; //My client id
int32_t SMPProfCount = 0; //Loaded SMP credential count
SMPProfile_t *SMPProfs = NULL; //SMP Connection credentials
int32_t SelectedSMPProf; //Current SMP profile ID
SMPPlayers_t SMPPlayerInfo[MAX_CLIENTS]; //Client information (Modified by server)
uint8_t TempRXBuffer[NET_BUFFER_SIZE]; //For packet size grantee
size_t TRXBLength; //For packet size grantee
int32_t NetworkTimeout = 100; //If there's still no packet after this period, client will auto disconnect.
int32_t TimeoutTimer;
int32_t ConnectionTimeoutTimer;
extern int32_t DifEnemyBaseCount[4];
extern int32_t DifEnemyBaseDist;
extern double DifATKGain;
int32_t DisconnectReasonProvided = 0; //To avoid erasing disconnect message
int32_t DebugSpam = 0; //If this set to 1, packets that sent so frequently, will be print.
extern int32_t InitSpawnRemain;
int32_t DisconnectWhenBadPacket = 1; //Need to be modified by gdb
char **BlockedUsers = NULL; //blocked username ptr array (dynamic alloc)
int32_t BlockedUsersCount = 0; //Elements of Blocked Users
int32_t IsChatEnable = 1; //0 to supress chat
int32_t WebsockMode; //If 1, client should not add length header in sending messages, always 0 in non WASM builds
extern int32_t GameState;

int32_t process_smp_events(uint8_t*, size_t, int32_t);
void net_server_send_cmd(server_command_t);
void net_message_handler(uint8_t*, size_t);
void connection_establish_handler();
void connection_close_handler();
int32_t evh_playable_location(uint8_t*, size_t, int32_t);
int32_t evh_place_item(uint8_t*, size_t, int32_t);
int32_t evh_use_skill(uint8_t*, size_t, int32_t);
ssize_t evh_chat(uint8_t*, size_t, int32_t);
int32_t evh_reset(uint8_t*, size_t, int32_t);
int32_t evh_change_atkgain(uint8_t*, size_t, int32_t);
ssize_t evh_hello(uint8_t*, size_t, int32_t);
int32_t evh_bye(int32_t);
int32_t evh_change_playable_id(uint8_t*, size_t, int32_t);
int32_t findblockeduser(char*);
int32_t addusermute(char*);
void process_tcp_packet(uint8_t*, size_t);
int32_t change_playable_location_of_cid(int32_t, double, double);

void close_connection_cmd() {
	info("Disconnect requested from user.\n");
	close_connection(NULL);
}

//Called every 10 ms, do network task.
void network_recv_task() {
	//If not connected, return
	if(SMPStatus == NETWORK_DISCONNECTED) {
		return;
	}

	if(SMPStatus != NETWORK_CONNECTING && NetworkTimeout != 0) {
		//Process in-connection timeout
		TimeoutTimer--;
		if(TimeoutTimer < 0) {
			warn("network_recv_task(): Timeout\n");
			close_connection( (char*)get_localized_string(TEXT_SMP_TIMEOUT) );
			return;
		}
	}

	#ifndef __WASM //in WASM version, they will process recv and connection timeout
	//If connect() is in progress
	if(SMPStatus == NETWORK_CONNECTING) {
		//Process connection timeout (5Sec)
		if(ConnectionTimeoutTimer > 500) {
			warn("network_recv_task(): Connection timed out.\n");
			close_connection( (char*)get_localized_string(TEXT_SMP_TIMEOUT) );
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
		return;
	} else if(r == -2) { //recv error
		warn("network_recv_task(): recv failed\n");
		close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
		return;
	}
	process_tcp_packet(b, (size_t)r);
	#endif
}

void process_tcp_packet(uint8_t *b, size_t l) {
	if(WebsockMode == 0) {
		TimeoutTimer = NetworkTimeout;
	}
	//Process data
	//Length 0-FD    XX
	//Length FE-FFFF FE XX XX
	//concat TempRXBuffer and b into tmpbuf
	uint8_t tmpbuf[NET_BUFFER_SIZE * 2];
	memcpy(tmpbuf, TempRXBuffer, TRXBLength);
	memcpy(&tmpbuf[TRXBLength], b, (size_t)l);
	size_t totallen = TRXBLength + (size_t)l;
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
			close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
			return;
		}
		if(remain < reqsize) {
			break; //Not enough packet received
		} else {
			net_message_handler(&tmpbuf[p + lenhdrsize], reqsize - lenhdrsize);
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
	chat( (char*)get_localized_string(8) ); //connected
	info("Connected to remote SMP server.\n");
	SMPStatus = NETWORK_CONNECTED;
	if(WebsockMode == 0) {
		send_tcp_socket("\n", 1);
		info("RAW mode: sending raw protocol request.\n");
	}
	TimeoutTimer = NetworkTimeout;
}

//Network message receiver handler
void net_message_handler(uint8_t *pkt, size_t plen) {
	if(WebsockMode == 1) {
		TimeoutTimer = NetworkTimeout;
	}
	//Handle packet
	if(pkt[0] == NP_RESP_DISCONNECT) {
		//Disconnect packet
		char reason[NET_BUFFER_SIZE];
		memcpy(reason, &pkt[1], plen - 1);
		reason[plen - 1] = 0;
		info("net_messsage_handler(): Disconnect request: %s\n", reason);
		close_connection(reason);
	} else if(pkt[0] == NP_GREETINGS) {
		//greetings response
		//Security update: avoid sending credentials for multiple times
		if(SMPcid != -1) {
			warn("net_message_handler(): Duplicate greetings packet.\n");
			if(DisconnectWhenBadPacket) {
				close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
			}
			return;
		}
		//Data length check
		if(plen != sizeof(np_greeter_t) ) {
			warn("net_message_handler(): Too short greetings packet.\n");
			if(DisconnectWhenBadPacket) {
				close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
			}
			return;
		}
		np_greeter_t *p = (np_greeter_t*)pkt;
		SMPcid = (int32_t)p->cid;
		memcpy(SMPsalt, p->salt, SALT_LENGTH);
		info("net_message_handler(): Server greeter received. CID=%d.\n", SMPcid);
		//Send credentials
		net_server_send_cmd(NP_LOGIN_WITH_HASH);
	} else if(pkt[0] == NP_RESP_LOGON) {
		//Login response: returns current userlist
		size_t p = 1;
		int32_t c = 0;
		while(p < plen && c < MAX_CLIENTS) {
			//Get CID
			int32_t cid = (int32_t) ( (int8_t)pkt[p]);
			size_t csiz = strnlen( (char*)&pkt[p + 1], UNAME_SIZE);
			//Length check
			if(csiz >= UNAME_SIZE) {
				warn("net_message_handler(): username too large (in initial user list)\n");
				close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
				return;
			}
			//Fill client information
			SMPPlayerInfo[c].cid = cid;
			SMPPlayerInfo[c].pid = 0;
			SMPPlayerInfo[c].respawn_timer = -1;
			SMPPlayerInfo[c].respawn_remain = -1;
			memcpy(SMPPlayerInfo[c].usr, &pkt[p + 1], csiz);
			SMPPlayerInfo[c].usr[csiz] = 0;
			c++;
			p += csiz + 2;
		}
		SMPStatus = NETWORK_LOGGEDIN;
		info("net_message_handler(): Login successful response.\n");
	} else if(pkt[0] == NP_EXCHANGE_EVENTS) {
		//Exchange event response
		if(plen - 1 >= NET_BUFFER_SIZE) {
			warn("net_message_handler(): GetEvent: Buffer overflow.\n");
			close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
		}
		//Get client coordinate list
		int32_t offs = 2;
		int32_t listlen = (uint8_t)pkt[1];
		for(int32_t i = 0; i < listlen; i++) {
			player_locations_table_t *t = (player_locations_table_t*)&pkt[offs];
			double tx = (double)network2host_fconv_16(t->px);
			double ty = (double)network2host_fconv_16(t->py);
			int32_t cid = t->cid;
			if(change_playable_location_of_cid(cid, tx, ty) == -1 && DisconnectWhenBadPacket) {
				close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
				return;
			}
			if(DebugSpam) {
				info("net_message_handler(): coords: %d X=%lf Y=%lf\n", cid, tx, ty);
			}
			offs += sizeof(player_locations_table_t);
		}
		memcpy(RXSMPEventBuffer, &pkt[offs], plen - offs);
		RXSMPEventLen = plen - offs;
		//printf("pkt_recv_handler(): GetEvent: RXSMPEventlen: %d\n", RXSMPEventLen);
	} else {
		if(DisconnectWhenBadPacket) {
			close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
		}
		warn("net_message_handler(): Unknown Packet (%ld): ", plen);
		for(size_t i = 0; i < plen; i++) {
			warn("%02x ", pkt[i]);
		}
		warn("\n");
	}
}

//Connect to specified address
int32_t connect_server_cmd(char* cmdparam) {
	if(GameState == GAMESTATE_TITLE) {
		return TEXT_UNAVAILABLE; //Unavailable in title screen
	}

	//Convert parameter to int
	int sp = atoi(cmdparam);

	//Check for parameter
	if(!is_range(sp, 0, SMPProfCount - 1) ) {
		warn("connect_server(): Bad profile number: %d\n", sp);
		return TEXT_UNAVAILABLE; //Unavailable if there's no profile
	}
	SelectedSMPProf = sp;

	//If already connected, return
	if(SMPStatus != NETWORK_DISCONNECTED) {
		warn("connect_server(): Already connected.\n");
		return TEXT_UNAVAILABLE; //Unavailable
	}

	//Announce and reset variables
	chatf("%s %s", get_localized_string(5), SMPProfs[sp].host); //Connecting
	info("connect_server(): Attempting to connect to %s:%s\n", SMPProfs[sp].host, SMPProfs[sp].port);
	RXSMPEventLen = 0;
	TXSMPEventLen = 0;
	TRXBLength = 0;
	SMPcid = -1;
	TimeoutTimer = 0;
	ConnectionTimeoutTimer = 0;
	DisconnectReasonProvided = 0;
	IsChatEnable = 1;
	#ifndef __WASM
	WebsockMode = 0;
	#endif
	for(int32_t i = 0; i < MAX_CLIENTS; i++) {
		SMPPlayerInfo[i].cid = -1;
		SMPPlayerInfo[i].playable_objid = -1;
	}
	SMPStatus = NETWORK_CONNECTING;

	//Connect
	if(make_tcp_socket(SMPProfs[sp].host, SMPProfs[sp].port) != 0) {
		SMPStatus = NETWORK_DISCONNECTED;
		return 6;//Can not connect
	}
	return -1; //OK
}

//Close connection but do not announce.
void close_connection(char* reason) {
	if(SMPStatus != NETWORK_DISCONNECTED) {
		if(DisconnectReasonProvided == 0) {
			if(reason != NULL) {
				chatf("%s %s", (char*)get_localized_string(TEXT_DISCONNECTED), reason);
			} else {
				chat( (char*)get_localized_string(TEXT_DISCONNECTED) );
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
		uint8_t ph[SHA512_LENGTH];
		if(compute_passhash(usr, pwd, SMPsalt, ph) != 0)  {
			warn("net_server_send_cmd(): NP_LOGIN_WITH_HASH: calculating hash failed.\n");
			return;
		}
		memcpy(&cmdbuf[1], ph, SHA512_LENGTH); //combine command and hash
		ctxlen = SHA512_LENGTH + 1;
	} else {
		warn("net_server_send_command(): Bad command.\n");
		return;
	}

	//Send it
	if(WebsockMode == 1) {
		//If websocket will ensure message length, send message without size header
		send_tcp_socket(cmdbuf, ctxlen);
		//info("websock mode: omitting sizehdr");
	} else {
		//Check for size and make size header, assemble message.
		size_t hdrlen;
		uint8_t buf[NET_BUFFER_SIZE];
		if(is_range( (int32_t)ctxlen, 0, 0xfd) ) {
			buf[0] = (uint8_t)ctxlen;
			hdrlen = 1;
		} else if(is_range( (int32_t)ctxlen, 0xfe, 0xffff) ) {
			uint16_t *p = (uint16_t*)&cmdbuf[1];
			buf[0] = 0xfe;
			*p = (uint16_t)host2network_fconv_16( (uint16_t)ctxlen);
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
		if(send_tcp_socket(buf, totallen) != totallen) {
			close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
		}
	}
}

//respawn foreign player
void respawn_smp_player(int cid) {
	int32_t r = spawn_playable(SMPPlayerInfo[cid].pid);
	SMPPlayerInfo[cid].playable_objid = r;
}

//Process SMP packet
void process_smp() {
	//Process respawn timer
	for(int32_t i = 0; i < MAX_CLIENTS; i++) {
		if(SMPPlayerInfo[i].cid != -1 && SMPPlayerInfo[i].respawn_timer > 0) {
			SMPPlayerInfo[i].respawn_timer--;
			//If timer went to 0, respawn playable character of SMP client
			if(SMPPlayerInfo[i].respawn_timer == 0) {
				SMPPlayerInfo[i].respawn_timer = -1;
				respawn_smp_player(i);
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
				if(DisconnectWhenBadPacket) {
					close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
				}
				return;
			}

			//decode header and check
			hdr = (event_hdr_t*)&RXSMPEventBuffer[ptr];
			int32_t cid = (int32_t)hdr->cid;
			size_t evlen = (size_t)network2host_fconv_16(hdr->evlen);
			size_t reclen = evlen + sizeof(event_hdr_t);
			if(!is_range(cid, -1, MAX_CLIENTS) ) {
				warn("process_smp(): Bad event header\n");
				if(DisconnectWhenBadPacket) {
					close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
					return;
				}
				break;
			}
			if(evlen >= NET_BUFFER_SIZE) {
				warn("process_smp(): Event chunk too large\n");
				if(DisconnectWhenBadPacket) {
					close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
					return;
				}
				break;
			}
			if(rem < reclen) {
				warn("process_smp(): Too short event chunk.\n");
				if(DisconnectWhenBadPacket) {
					close_connection( (char*)get_localized_string(TEXT_SMP_ERROR) );
					return;
				}
				break;
			}	
		
			//Process event
			uint8_t* evhead = &RXSMPEventBuffer[ptr + (int32_t)sizeof(event_hdr_t)];
			if(process_smp_events(evhead, evlen, cid) == -1 && DisconnectWhenBadPacket) {
				close_connection( (char*)get_localized_string(TEXT_SMP_ERROR));
				return;
			}
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
		size_t chatplen = strnlen(ctx, NET_CHAT_LIMIT);
		ev_chat_t ev = {
			.evtype = etype,
			.dstcid = -1
		};
		if(is_range( (int32_t)chatplen, 0, NET_CHAT_LIMIT - 1) ) {
			pktlen = sizeof(ev_chat_t) + chatplen + 1;
		} else {
			warn("net_stack_packet(): CHAT: too long message\n");
			pktlen = 0;
		}
		if(TXSMPEventLen + pktlen < NET_BUFFER_SIZE) {
			memcpy(poff, &ev, sizeof(ev_chat_t) );
			memcpy(&poff[sizeof(ev_chat_t)], ctx, chatplen);
			poff[sizeof(ev_chat_t) + chatplen] = 0;
		}
	} else if(etype == EV_RESET) {
		//Game round reset event
		ev_reset_t ev = {
			.evtype = etype,
			.level_seed = host2network_fconv_32(rand() ),
			.basedistance = host2network_fconv_16( (int16_t)DifEnemyBaseDist),
			.endlessmode = EndlessMode,
			.basecount0 = DifEnemyBaseCount[0],
			.basecount1 = DifEnemyBaseCount[1],
			.basecount2 = DifEnemyBaseCount[2],
			.basecount3 = DifEnemyBaseCount[3],
			.atkgain = (float)DifATKGain,
			.spawnlimit = (int8_t)InitSpawnRemain
		};
		pktlen = sizeof(ev_reset_t);
		if(TXSMPEventLen + pktlen < NET_BUFFER_SIZE) {
			memcpy(poff, &ev, sizeof(ev_reset_t) );
		}
	} else if(etype == EV_CHANGE_PLAYABLE_ID) {
		//playable character id change event
		ev_changeplayableid_t ev = {
			.evtype = etype,
			.pid = (uint8_t)CurrentPlayableID
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
	} else {
		TXSMPEventLen += pktlen;
	}
}

int32_t change_playable_location_of_cid(int cid, double tx, double ty) {
	//Parameter check
	if(!is_range_number(tx, 0, MAP_WIDTH) || !is_range_number(ty, 0, MAP_HEIGHT) || !is_range_number(cid, 0, MAX_CLIENTS) ) {
		if(DebugSpam) {
			warn("change_playable_location_of_cid(): Bad parameter\n");
		}
		return -1;
	}

	//Obtain client information
	int32_t i = lookup_smp_player_from_cid(cid);
	if(i == -1) {
		if(DebugSpam) {
			warn("change_playable_location_of_cid(): CID%d is not registered yet.\n", cid);
		}
		return 0;
	}

	//If they have valid playable character, change its coordinate.
	int32_t p_objid = SMPPlayerInfo[i].playable_objid;
	if(is_range(p_objid, 0, MAX_OBJECT_COUNT - 1) && p_objid != PlayingCharacterID) {
		Gobjs[p_objid].x = tx;
		Gobjs[p_objid].y = ty;
	} else if(DebugSpam) {
		warn("change_playable_location_of_cid(): Character of CID%d is not generated yet.\n", cid); //causes logspam, commented out
	}
	return 0;

}

//Process smp events (evbuf: content, evlen: length, cid: event owner id)
int32_t process_smp_events(uint8_t* evbuf, size_t evlen, int32_t cid) {
	//Decode packet
	size_t evp = 0;
	while(evp < evlen) {
		ssize_t remaining = (ssize_t)evlen - (ssize_t)evp;
		if(evbuf[evp] == EV_PLACE_ITEM) {
			//item placed
			if(remaining < sizeof(ev_placeitem_t) ) {
				warn("process_smp_events(): Too short EV_PLACE_ITEM packet, decoder will terminate.\n");
				return -1;
			}
			if(evh_place_item(evbuf, evp, cid) == -1 && DisconnectWhenBadPacket) {
				return -1;
			}
			evp += sizeof(ev_placeitem_t);

		} else if(evbuf[evp] == EV_USE_SKILL) {
			//playable skill used
			if(remaining < sizeof(ev_useskill_t) ) {
				warn("process_smp_events(): Too short EV_USE_SKILL packet, decoder will terminate.\n");
				return -1;
			}
			if(evh_use_skill(evbuf, evp, cid) == -1 && DisconnectWhenBadPacket) {
				return -1;
			}
			evp += sizeof(ev_useskill_t);

		} else if(evbuf[evp] == EV_CHAT) {
			//chat
			if(remaining < sizeof(ev_chat_t) ) {
				warn("process_smp_events(): Too short EV_CHAT packet, decoder will terminate.\n");
				return -1;
			}
			ssize_t pktsiz = evh_chat(evbuf, evp, cid);
			if(pktsiz == -1) {
				return -1;
			}
			evp += pktsiz;
			
		} else if(evbuf[evp] == EV_RESET) {
			//round reset request
			if(remaining < sizeof(ev_reset_t) ) {
				warn("process_smp_events(): Too short EV_RESET packet, decoder will terminate.\n");
				return -1;
			}
			if(evh_reset(evbuf, evp, cid) == -1 && DisconnectWhenBadPacket) {
				return -1;
			}
			evp += sizeof(ev_reset_t);

		} else if(evbuf[evp] == EV_HELLO) {
			//Another client connect event
			ssize_t pktsiz = evh_hello(evbuf, evp, cid);
			if(pktsiz == -1) {
				return -1;
			}
			evp += pktsiz;

		} else if(evbuf[evp] == EV_CHANGE_ATKGAIN) {
			//Change AtkGain Parameter request
			if(remaining < sizeof(ev_changeatkgain_t) ) {
				warn("process_smp_events(): Too short EV_CHANGE_ATKGAIN packet, decoder will terminate.\n");
				return -1;
			}
			if(evh_change_atkgain(evbuf, evp, cid) == -1 && DisconnectWhenBadPacket) {
				return -1;
			}
			evp += sizeof(ev_changeatkgain_t);

		} else if(evbuf[evp] == EV_BYE) {
			//Another client disconnect event
			if(evh_bye(cid) == -1 && DisconnectWhenBadPacket) {
				return -1;
			}
			evp += 1;

		} else if(evbuf[evp] == EV_CHANGE_PLAYABLE_ID) {
			//Playable id change event
			if(remaining < sizeof(ev_changeplayableid_t) ) {
				warn("process_smp_events(): Too short EV_CHANGE_PLAYABLE_ID packet, decoder will terminate.\n");
				return -1;
			}
			if(evh_change_playable_id(evbuf, evp, cid) == -1 && DisconnectWhenBadPacket) {
				return -1;
			}
			evp += sizeof(ev_changeplayableid_t);

		} else {
			warn("process_smp_events(): Unknown event type, decoder terminated.\n");
			return -1;
		}
	}
	return 0;
}

//Lookup smp client information id from client id
int32_t lookup_smp_player_from_cid(int32_t cid) {
	if(!is_range(cid, 0, MAX_CLIENTS) ) {
		return -1;
	}

	for(int32_t i = 0; i < MAX_CLIENTS; i++) {
		if(SMPPlayerInfo[i].cid == cid) {
			return i;
		}
	}
	return -1;
}

//EV_PLACE_ITEM packet handler
int32_t evh_place_item(uint8_t* eventbuffer, size_t eventoff, int cid) {
	ev_placeitem_t *ev = (ev_placeitem_t*)&eventbuffer[eventoff];
	obj_type_t tid = (obj_type_t)ev->tid;
	double x = (double)network2host_fconv_16(ev->x);
	double y = (double)network2host_fconv_16(ev->y);
	info("evh_place_item(): cid=%d, tid=%d, x=%f, y=%f\n", cid, tid, x, y);
	
	//Parameter check
	if(!is_range_number(x, 0, MAP_WIDTH) || !is_range_number(y, 0, MAP_HEIGHT) || !is_range(tid, 0, MAX_TID - 1) ) {
		warn("evh_place_item(): packet check failed.\n");
		return -1;
	}

	//Get sender client's playable character object id
	int32_t i = lookup_smp_player_from_cid(cid);
	if(i == -1) {
		warn("evh_place_item(): CID%d is not in client list yet!\n", cid);
		return 0;
	}
	int32_t p_objid = SMPPlayerInfo[i].playable_objid;
	if(!is_range(p_objid, 0, MAX_OBJECT_COUNT - 1) ) {
		warn("evh_place_item(): packet ignored, CID%d does not have playable character.\n", cid);
		return 0;
	}

	add_character(tid, x, y, i);
	return 0;
}

//EV_USE_SKILL packet handler
int32_t evh_use_skill(uint8_t* eventbuffer, size_t eventoff, int32_t cid) {
	ev_useskill_t *ev = (ev_useskill_t*)&eventbuffer[eventoff];
	uint8_t skillid = ev->skillid;
	info("evh_use_skill(): skillid=%d, cid=%d\n", skillid, cid);

	//Parameter check
	if(!is_range(skillid, 0, SKILL_COUNT - 1) ) {
		warn("evh_use_skill(): packet check failed.\n");
		return -1;
	}
	
	//Get playable character objid of the sender client
	int32_t i = lookup_smp_player_from_cid(cid);
	if(i == -1) {
		warn("evh_use_skill(): cid%d is not in client list yet.\n", cid);
		return 0;
	}
	int32_t playable_objid = SMPPlayerInfo[i].playable_objid;
	if(!is_range(playable_objid, 0, MAX_OBJECT_COUNT - 1) ) {
		warn("evh_use_skill(): User cid%d has invalid playable_objid.\n", cid);
		return 0;
	}
	int32_t pid = SMPPlayerInfo[i].pid;
	if(!is_range(pid, 0, PLAYABLE_CHARACTERS_COUNT - 1) ) {
		warn("evh_use_skill(): User cid%d has invalid pid (playable character id)\n", cid);
		return 0;
	}

	//Use skill as remote player
	PlayableInfo_t plinf;
	lookup_playable(pid, &plinf);
	use_skill(playable_objid, skillid, plinf);
	return 0;
}

//EV_CHAT packet handler
ssize_t evh_chat(uint8_t* evbuffer, size_t eventoff, int32_t cid) {
	ev_chat_t *ev = (ev_chat_t*)&evbuffer[eventoff];
	char* netchat = (char*)&evbuffer[eventoff + sizeof(ev_chat_t)];
	int32_t dstcid = ev->dstcid;
	size_t clen = strnlen(netchat, NET_CHAT_LIMIT);
	info("evh_chat(): clen=%ld, dstcid=%d, cid=%d\n", clen, dstcid, cid);
	
	//Parameter check
	if(clen >= NET_CHAT_LIMIT) {
		warn("evh_chat(): Too long chat packet\n");
		return -1;
	}
	if(!is_range(dstcid, -1, MAX_CLIENTS) ) {
		warn("evh_chat(): Bad dstcid!\n");
		return -1;
	}

	//Show chat
	char ctx[NET_CHAT_LIMIT];
	memcpy(ctx, netchat, clen);
	ctx[clen] = 0; //Terminate string with NUL
	char chathdr[BUFFER_SIZE];
	char w[] = "private ";
	int32_t isblocked = 0;
	if(dstcid == -1) {
		w[0] = 0;
	}
	if(cid == -1) {
		snprintf(chathdr, BUFFER_SIZE, "[Server]");
	} else {
		int32_t t = lookup_smp_player_from_cid(cid);
		if(t != -1) {
			char* usr = SMPPlayerInfo[t].usr;
			snprintf(chathdr, BUFFER_SIZE, "%s<%s>",w ,usr);
			if(findblockeduser(usr) != -1) {
				isblocked = 1;
			}
		} else {
			info("unknown user chat: %s\n", ctx);
			return clen + sizeof(ev_chat_t) + 1;
		}
	}
	if(IsChatEnable && !isblocked) {
		chatf("%s %s", chathdr, ctx);
	}
	return clen + sizeof(ev_chat_t) + 1;
}

//EV_RESET handler
int32_t evh_reset(uint8_t* eventbuffer, size_t eventoffset, int32_t cid) {
	ev_reset_t *ev = (ev_reset_t*)&eventbuffer[eventoffset];
	uint32_t _seed = (uint32_t)network2host_fconv_32(ev->level_seed);
	int32_t basedistance = (int32_t)network2host_fconv_16(ev->basedistance);
	double atkgain = (double)ev->atkgain;
	int32_t spawnlimit = (int32_t)ev->spawnlimit;
	int32_t em = (int32_t)ev->endlessmode;
	info("evh_reset(): seed=%u spawnlimit=%d basedistance=%d atkgain=%lf BaseCount: %d, %d, %d, %d endless=%d cid=%d\n", _seed, spawnlimit, basedistance, atkgain, ev->basecount0, ev->basecount1, ev->basecount2, ev->basecount3, em, cid);

	//Check paraneter
	if(!is_range(basedistance, MIN_EBDIST, MAX_EBDIST) || !is_range_number(atkgain, MIN_ATKGAIN, MAX_ATKGAIN) || !is_range(spawnlimit, -1, MAX_SPAWN_COUNT) ) {
		warn("evh_reset(): bad packet!\n");
		return -1;
	} 

	//Change map difficulty parameter and reset round
	EndlessMode = em;
	DifEnemyBaseDist = basedistance;
	DifATKGain = atkgain;
	DifEnemyBaseCount[0] = ev->basecount0;
	DifEnemyBaseCount[1] = ev->basecount1;
	DifEnemyBaseCount[2] = ev->basecount2;
	DifEnemyBaseCount[3] = ev->basecount3;
	InitSpawnRemain = spawnlimit;
	for(int32_t i = 0; i < MAX_CLIENTS; i++) {
		SMPPlayerInfo[i].respawn_remain = spawnlimit;
	}
	srand(_seed);
	info("evh_reset(): Round reset request\n");
	reset_game();
	return 0;
}

//EV_PLAYABLE_LOCATION packet handler
int32_t evh_playable_location(uint8_t* eventbuffer, size_t eventoff, int32_t cid) {
	ev_playablelocation_t *ev = (ev_playablelocation_t*)&eventbuffer[eventoff];
	double tx = (double)network2host_fconv_16(ev->x);
	double ty = (double)network2host_fconv_16(ev->y);

	if(DebugSpam) {
		info("evh_playable_location(): x=%lf, y=%lf, cid=%d\n", tx, ty, cid);
	}
	
	//Parameter check
	if(!is_range_number(tx, 0, MAP_WIDTH) || !is_range_number(ty, 0, MAP_HEIGHT) ) {
		if(DebugSpam) {
			warn("evh_playable_location(): Bad packet.\n");
		}
		return -1;
	}

	//Obtain client information
	int32_t i = lookup_smp_player_from_cid(cid);
	if(i == -1) {
		if(DebugSpam) {
			warn("evh_playable_location(): CID%d is not registered yet.\n", cid);
		}
		return 0;
	}

	//If they have valid playable character, change its coordinate.
	int32_t p_objid = SMPPlayerInfo[i].playable_objid;
	if(is_range(p_objid, 0, MAX_OBJECT_COUNT - 1) ) {
		Gobjs[p_objid].x = tx;
		Gobjs[p_objid].y = ty;
	} else if(DebugSpam) {
		warn("evh_playable_location: Character of CID%d is not generated yet.\n", cid); //causes logspam, commented out
	}
	return 0;
}

//EV_BYE handler
int32_t evh_bye(int32_t cid) {
	info("evh_bye(): cid=%d\n", cid);

	//Security check
	if(!is_range(cid, 0, MAX_CLIENTS) ) {
		warn("evh_bye(): bad packet.\n");
		return -1;
	}

	//Leaving client have to have own playable character
	int32_t i = lookup_smp_player_from_cid(cid);
	if(i == -1) {
		warn("evh_bye(): CID%d is not on client list.\n", cid);
		return 0;
	}

	//Notify disconnection of another client
	chatf("%s %s", SMPPlayerInfo[i].usr, get_localized_string(11) );
	//Clean up left user's playable character
	int32_t j = SMPPlayerInfo[i].playable_objid;
	info("Trying to clear playable object id=%d\n", j);
	if(is_range(j, 0, MAX_OBJECT_COUNT - 1) ) {
		Gobjs[j].tid = TID_NULL;
	} else {
		warn("evh_bye(): CID%d does not have playable character.\n", cid);
	}
	return 0;
}

//EV_CHANGE_ATKGAIN handler
int32_t evh_change_atkgain(uint8_t* eventbuffer, size_t eventoffset, int32_t cid) {
	ev_changeatkgain_t *ev = (ev_changeatkgain_t*)&eventbuffer[eventoffset];
	double atkgain = (double)ev->atkgain;
	info("evh_change_atkgain(): atkgain=%lf, cid=%d\n", atkgain, cid);

	//Check param
	if(!is_range_number(atkgain, MIN_ATKGAIN, MAX_ATKGAIN) ) {
		warn("evh_change_atkgain: wrong parameter!");
		return -1;
	}

	//Change atkgain parameter
	DifATKGain = atkgain;
	info("evh_change_atkgain(): Atkgain changed to %f\n", atkgain);
	return 0;
}

//EV_HELLO handler
ssize_t evh_hello(uint8_t *eventbuffer, size_t eventoffset, int cid) {
	uint8_t *evh = &eventbuffer[eventoffset];
	size_t namelen = strnlen( (char*)&evh[1], UNAME_SIZE);
	info("evh_hello(): namelen=%ld, cid=%d\n", namelen, cid);
	
	//Check packet
	if(!is_range(cid, 0, MAX_CLIENTS) ) {
		warn("evh_hello(): bad packet.\n");
		return -1;
	}
	if(namelen >= UNAME_SIZE) {
		warn("evh_hello(): packet username too large.\n");
		return -1;
	}

	//Announce new client and append to client info
	char funame[UNAME_SIZE];
	memcpy(funame, &evh[1], namelen);
	funame[namelen] = 0;
	chatf("%s %s", funame, get_localized_string(10) );
	//Register client info
	for(int32_t i = 0; i < MAX_CLIENTS; i++) {
		if(SMPPlayerInfo[i].cid == -1 || SMPPlayerInfo[i].cid == cid) {
			SMPPlayerInfo[i].cid = cid;
			SMPPlayerInfo[i].pid = 0;
			SMPPlayerInfo[i].playable_objid = OBJID_INVALID;
			SMPPlayerInfo[i].respawn_timer = -1;
			SMPPlayerInfo[i].respawn_remain = -1;
			strcpy(SMPPlayerInfo[i].usr, funame);
			break;
		}
	}
	return namelen + 2;
}

//EV_CHANGE_PLAYABLE_ID handler
int32_t evh_change_playable_id(uint8_t* eventbuffer, size_t eventoffset, int32_t cid) {
	ev_changeplayableid_t *ev = (ev_changeplayableid_t*)&eventbuffer[eventoffset];
	int32_t pid = ev->pid;
	info("evh_change_playable_id(): pid=%d, cid=%d\n", pid, cid);

	//Param check
	if(!is_range(pid, 0, PLAYABLE_CHARACTERS_COUNT - 1) ) {
		warn("evh_change_playable_id(): parameter check failed.\n");
		return -1;
	}
	
	//change playable character id of the sender.
	int32_t i = lookup_smp_player_from_cid(cid);
	if(i != -1) {
		SMPPlayerInfo[i].pid = pid;
	} else {
		warn("evh_change_playable_id(): CID%d is not on client list.\n", cid);
	}
	return 0;
}


int32_t changetimeout_cmd(char *param) {
	//change network timeout
	int32_t i = (int32_t)strtol(param, NULL, 10);
	if(!is_range(i, 0, 1000) ) {
		warn("changetimeout_cmd(): paraneter must be 0 to 1000.\n");
		return TEXT_BAD_COMMAND_PARAM; //Bad param
	}
	NetworkTimeout = i;
	return -1;
}

int32_t getcurrentsmp_cmd() {
	//Command to get connection state
	if(SMPStatus == NETWORK_DISCONNECTED) {
		return TEXT_OFFLINE; //Offline
	} else {
		chatf("getcurrentsmp: %d (%s@%s:%s)", SelectedSMPProf, SMPProfs[SelectedSMPProf].usr, SMPProfs[SelectedSMPProf].host, SMPProfs[SelectedSMPProf].port);
	}
	return -1;
}

int32_t add_smp_cmd(char* p) {
	//Add SMP profile
	if(add_smp_profile(p, ' ') != 0) {
		return TEXT_BAD_COMMAND_PARAM; //Bad parameter
	}
	return -1;
}

int32_t get_smp_cmd(char *param) {
	//Get current selected SMP profile
	int i = atoi(param);
	if(is_range(i, 0, SMPProfCount - 1) ) {
		chatf("getsmp: %d (%s@%s:%s)", i, SMPProfs[i].usr, SMPProfs[i].host, SMPProfs[i].port);
	} else {
		warn("get_smp_cmd(): bad SMP id.\n");
		return TEXT_BAD_COMMAND_PARAM; //Bad parameter
	}
	return -1;
}

int32_t add_smp_profile(char* line, char spliter) {
	SMPProfile_t t_rec;
	char *t = line;
	for(int32_t i = 0; i < 4; i++) {
		const size_t rec_size[] = {HOSTNAME_SIZE, PORTNAME_SIZE, UNAME_SIZE, PASSWD_SIZE};
		char *rec_ptr[] = {t_rec.host, t_rec.port, t_rec.usr, t_rec.pwd};
		//Find splitter letter
		char *t2 = strchr(t, spliter);
		//Last record will be finished by newline character
		if(i == 3) {
			t2 = strchr(t, '\n');
			if(t2 == NULL) {
				t2 = line + strlen(line);
			}
		}
		if(t2 == NULL) {
			warn("add_smp_profile(): No splitter letter.\n");
			return -1;
		}

		//Measure distance to the letter
		size_t reclen = (size_t)t2 - (size_t)t;
		if(reclen >= rec_size[i]) {
			warn("add_smp_profile(): Record size overflow.\n");
			return -1;
		}
		if(reclen == 0) {
			warn("add_smp_profile(): Empty field detected.\n");
			return -1;
		}

		//Copy string from t to the splitter letter into elem
		memcpy(rec_ptr[i], t, reclen);
		rec_ptr[i][reclen] = 0;
		t = &t2[1]; //Set next record finding position right after the splitter letter
	}
	info("SMP Credential append (ID%d): Host: %s, Port: %s, User: %s\n", SMPProfCount, t_rec.host, t_rec.port, t_rec.usr);
	
	//append to SMPProfs
	if(SMPProfs == NULL) {
		SMPProfs = malloc(sizeof(SMPProfile_t) );
		if(SMPProfs == NULL) {
			warn("add_smp_profile(): insufficient memory, malloc failure.\n");
			return -1;
		}
	} else {
		SMPProfile_t *t = realloc(SMPProfs, sizeof(SMPProfile_t) * (size_t)(SMPProfCount + 1) );
		if(t == NULL) {
			warn("add_smp_profile(): insufficient memory, realloc failure.\n");
			return -1;
		}
		SMPProfs = t;
	}
	memcpy(&SMPProfs[SMPProfCount], &t_rec, sizeof(SMPProfile_t) );
	SMPProfCount++;
	return 0;
}

int32_t getclients_cmd() {
	//Get current connected users
	if(SMPStatus == NETWORK_LOGGEDIN) {
		//Make client username list divided by ,
		char clientlist[BUFFER_SIZE] = "";
		size_t p = 0;
		for(int32_t i = 0; i < MAX_CLIENTS; i++) {
			if(SMPPlayerInfo[i].cid != -1) {
				char t[UNAME_SIZE + 5];
				ssize_t s;
				s = snprintf(t, UNAME_SIZE + 5, "%s (%d), ", SMPPlayerInfo[i].usr, SMPPlayerInfo[i].cid);
				if(s < 0) {
					warn("getclients_cmd(): error.\n");
					return TEXT_UNAVAILABLE;
				}
				if(p + s >= UNAME_SIZE + 5) {
					warn("getclients(): overflow condition while making list.\n");
					return TEXT_UNAVAILABLE;
				}
				memcpy(&clientlist[p], t, s);
				p += s;
			}
		}
		clientlist[p - 2] = 0;
		chatf("getclients: %s", clientlist);
	} else {
		warn("getclients_cmd(): disconnected.\n");
		return TEXT_OFFLINE; //Offline
	}
	return -1;
}

//Find specified name from BlockedUsers array, return id if match (otherwise return -1)
int32_t findblockeduser(char *n) {
	//Search for specified string
	for(int32_t i = 0; i < BlockedUsersCount; i++) {
		char *e = BlockedUsers[i];
		if(e == NULL) {
			continue;
		}
		if(strcmp(n, e) == 0) {
			return i;
		}
	}
	return -1;
}

int32_t addusermute_cmd(char *p) {
	size_t l = strnlen(p, UNAME_SIZE);
	//Check param
	if(l >= UNAME_SIZE) {
		warn("addusermute_cmd(): too long username.\n");
		return TEXT_BAD_COMMAND_PARAM; // Bad parameter
	}
	if(addusermute(p) == -1) {
		return TEXT_UNAVAILABLE;
	}
	return -1;
}

int32_t addusermute(char *p) {
	size_t l = strnlen(p, UNAME_SIZE);

	if(l >= UNAME_SIZE) {
		warn("addusermute(): too long username\n");
		return -1;
	}

	//Check for dup
	if(findblockeduser(p) != -1) {
		warn("addusermute(): You already have that.\n");
		return -1;
	}
	
	//If it is first time to add, malloc. if there's NULL in array, overwrite it. if no space, grow the array.
	int32_t i = -1;
	if(BlockedUsers == NULL) {
		BlockedUsers = malloc( sizeof(char*) );
		if(BlockedUsers == NULL) {
			warn("addusermute(): malloc() failed.\n");
			return -1;
		}
		BlockedUsersCount++;
		i = 0;
	} else {
		//Find NULL
		for(int32_t j = 0; j < BlockedUsersCount; j++) {
			if(BlockedUsers[j] == NULL) {
				i = j;
				break;
			}
		}
		//Grow if there's no NULL
		if(i == -1) {
			char **t = realloc(BlockedUsers, sizeof(char*) * (BlockedUsersCount + 1) );
			if(t == NULL) {
				warn("addusermute(): malloc() failed.\n");
				return -1;
			}
			BlockedUsers = t;
			i = BlockedUsersCount;
			BlockedUsersCount++;
		}
	}

	//Store value
	if(is_range(i, 0, BlockedUsersCount - 1) ) {
		BlockedUsers[i] = malloc(l + 1);
		if(BlockedUsers[i] == NULL) {
			warn("addusermute(): malloc() failed.\n");
			return -1;
		}
		strcpy(BlockedUsers[i], p);
		info("addusermute(): muted user added: %s\n", p);
		return 0;
	} else {
		warn("addusermute(): You made buggy code!!\n");
		return -1;
	}
}

int32_t delusermute_cmd(char *p) {
	//Unmute user command
	size_t l = strnlen(p, UNAME_SIZE);

	//Check param
	if(l >= UNAME_SIZE) {
		warn("addusermute_cmd(): too long username.\n");
		return TEXT_BAD_COMMAND_PARAM; //Bad parameter
	}
	
	//Search for specified string
	int32_t i = findblockeduser(p);
	if(is_range(i, 0, BlockedUsersCount - 1) ) {
		//If find, free it and make it NULL
		info("delusermute_cmd(): muted user removed: %s\n", BlockedUsers[i]);
		free(BlockedUsers[i]);
		BlockedUsers[i] = NULL;
	} else {
		warn("addusermute_cmd(): not found on list!\n");
		return TEXT_BAD_COMMAND_PARAM;
	}
	return -1;
}

int32_t listusermute_cmd() {
	//list muted users
	if(BlockedUsers == NULL || BlockedUsersCount == 0) {
		return TEXT_UNAVAILABLE;
	}

	//Make list
	char t[BUFFER_SIZE];
	size_t p = 0;
	int32_t c = 0;
	for(int32_t i = 0; i < BlockedUsersCount; i++) {
		char *e = BlockedUsers[i];
		if(e == NULL) {
			continue;
		}
		size_t l = strnlen(BlockedUsers[i], UNAME_SIZE);
		if(l >= UNAME_SIZE) {
			continue;
		}
		//Check array boundary
		if(p + l + 2 >= BUFFER_SIZE) {
			warn("listusermute_cmd(): overflow condition\n");
			return TEXT_UNAVAILABLE;
		}
		memcpy(&t[p], BlockedUsers[i], l);
		//Terminate last element with 0, else append ", "
		if(i == BlockedUsersCount - 1) {
			t[p + l] = 0;
		} else {
			memcpy(&t[p + l], ", ", 2);
		}
		p += l + 2;
		c++;
	}

	chatf("listmuted (%d): %s", c, t);
	return -1;
}

int32_t togglechat_cmd() {
	//Chat toggle command
	if(IsChatEnable) {
		IsChatEnable = 0;
		return 15; //Chat disable
	} else {
		IsChatEnable = 1;
		return 14; //Chat enable
	}
}
