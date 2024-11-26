/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by Gtk4

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

gamesys.c: game process and related functions
*/

#include "main.h"

//For packet data conversion
#ifndef WIN32
	#include <arpa/inet.h>
#else
	#include <winsock.h>
#endif


extern char ChatMessages[MAX_CHAT_COUNT][BUFFER_SIZE];
extern int32_t ChatTimeout;
extern GameObjs_t Gobjs[MAX_OBJECT_COUNT];
extern int32_t CameraX, CameraY;
extern int32_t CursorX, CursorY;
extern obj_type_t AddingTID;
extern int32_t CommandCursor; //Command System Related
extern char CommandBuffer[BUFFER_SIZE];
gboolean CommandBufferMutex = FALSE;
extern GdkClipboard* GClipBoard;
extern gboolean DebugMode;
extern gamestate_t GameState;
extern int32_t StateChangeTimer;
extern int32_t PlayingCharacterID;
extern int32_t Money;
extern int32_t EarthID;
extern gboolean CharacterMove;
extern int32_t SelectingItemID;
extern const int32_t ITEMPRICES[ITEM_COUNT];
extern const int32_t FTIDS[ITEM_COUNT];
//extern int32_t ErrorShowTimer;
//extern int32_t RecentErrorId;
extern langid_t LangID;
extern int32_t ItemCooldownTimers[ITEM_COUNT];
extern int32_t ITEMCOOLDOWNS[ITEM_COUNT];
extern int32_t SkillCooldownTimers[SKILL_COUNT];
extern int32_t CurrentPlayableCharacterID;
extern keyflags_t KeyFlags;
extern gboolean ProgramExiting;
extern int32_t MapTechnologyLevel;
extern int32_t SKILLCOOLDOWNS[SKILL_COUNT];
extern int32_t MapEnergyLevel;
//extern LOLSkillKeyState_t SkillKeyStates[SKILL_COUNT];
extern int32_t SkillKeyState;
extern int32_t SubthreadMessageReq;
char SubthreadMSGCTX[BUFFER_SIZE];
extern smpstatus_t SMPStatus;
extern size_t RXSMPEventLen, TXSMPEventLen;
extern uint8_t RXSMPEventBuffer[SMP_EVENT_BUFFER_SIZE], TXSMPEventBuffer[SMP_EVENT_BUFFER_SIZE];
extern int32_t SMPcid;
extern int32_t Difficulty;
extern int32_t SMPProfCount;
extern SMPProfile_t *SMPProfs;
extern int32_t SelectedSMPProf;

//Request to show chat message from another thread (this can not be nested)
void chat_request(char* ctx) {
	strncpy(SubthreadMSGCTX, ctx, BUFFER_SIZE);
	SubthreadMSGCTX[BUFFER_SIZE - 1] = 0; //for additional security
	SubthreadMessageReq = 1;
}

//Request to show chat message from another thread, but formatstr (this can not be nested)
void chatf_request(const char* ctx, ...) {
	va_list varg;
	char t[BUFFER_SIZE];
	va_start(varg, ctx);
	vsnprintf(t, BUFFER_SIZE, ctx, varg);
	va_end(varg);
	t[BUFFER_SIZE - 1] = 0; //for additional security
	chat_request(t);
}

//Translate local coordinate into global coordinate
void local2map(double localx, double localy, double* mapx, double* mapy) {
	*mapx = CameraX + localx;
	*mapy = CameraY + (WINDOW_HEIGHT - localy);
}

void getlocalcoord(int32_t i, double *x, double *y) {
	if(!is_range(i, 0, MAX_OBJECT_COUNT - 1)){
		die("getlocalcoord(): bad parameter passed!\n");
		return;
	}
	*x = Gobjs[i].x - CameraX;
	*y = WINDOW_HEIGHT - (Gobjs[i].y - CameraY);
}

//Add character into game object buffer, returns -1 if fail.
//tid (character id), x, y(initial pos), parid (parent object id)
int32_t add_character(obj_type_t tid, double x, double y, int32_t parid) {
	int32_t newid = -1;
	//Get empty slot id
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		if(Gobjs[i].tid != TID_NULL) { continue; } //Skip occupied slot
		newid = i;
		break;
	}
	if(newid == -1) {
		die("gamesys.c: add_character(): Gameobj is full!\n");
		return 0;
	}
	//Add new character
	LookupResult_t t;
	lookup(tid, &t);
	Gobjs[newid].imgid = t.initimgid;
	Gobjs[newid].tid = tid;
	Gobjs[newid].x = x;
	Gobjs[newid].y = y;
	Gobjs[newid].hp = (double)t.inithp;
	Gobjs[newid].sx = 0;
	Gobjs[newid].sy = 0;
	Gobjs[newid].aiming_target = OBJID_INVALID;
	Gobjs[newid].cid = -1;
	for(uint8_t i = 0; i < CHARACTER_TIMER_COUNT; i++) {
		Gobjs[newid].timers[i] = 0;
	}
	Gobjs[newid].hitdiameter = t.inithitdiameter;
	Gobjs[newid].timeout = t.timeout;
	Gobjs[newid].damage = t.damage;
	//set srcid
	if(is_range(parid, 0, MAX_OBJECT_COUNT - 1) ) {
		//Set src id to parid if Gobjs[parid] is UNITTYPE_UNIT or UNITTYPE_FACILITY
		//Otherwise inherit from parent.
		if(is_range(Gobjs[parid].tid, 0, MAX_TID - 1) ) {
			LookupResult_t t2;
			lookup(Gobjs[parid].tid, &t2);
			if(t2.objecttype == UNITTYPE_UNIT || t2.objecttype == UNITTYPE_FACILITY) {
				Gobjs[newid].srcid = parid;
			} else {
				Gobjs[newid].srcid = Gobjs[parid].srcid;
			}
		} else {
			Gobjs[newid].srcid = Gobjs[parid].srcid;
		}
	} else {
		Gobjs[newid].srcid = -1;
	}
	Gobjs[newid].parentid = parid;
	//MISSLIE, bullet and laser will try to aim closest enemy unit
	if(tid == TID_MISSILE || tid == TID_ALLYBULLET || tid == TID_ENEMYBULLET || tid == TID_KUMO9_X24_MISSILE) {
		//Set initial target
		int32_t r;
		if(tid == TID_KUMO9_X24_MISSILE) {
			r = find_random_unit(newid, KUMO9_X24_MISSILE_RANGE, UNITTYPE_FACILITY | UNITTYPE_UNIT);
		} else {
			r = find_nearest_unit(newid, DISTANCE_INFINITY, UNITTYPE_FACILITY | UNITTYPE_BULLET_INTERCEPTABLE | UNITTYPE_UNIT);
		}
		if(r != -1) {
			Gobjs[newid].aiming_target = r;
			set_speed_for_following(newid);
		} else {
			Gobjs[newid].tid = TID_NULL;
			g_print("add_character(): object destroyed, no target found.\n");
		}
	}
	switch(tid) {
	case TID_EARTH:
		Gobjs[newid].hp = 1; //For initial scene
		break;
	case TID_ENEMYBASE:
		//tid = 1, zundamon star
		//find earth and attempt to approach it
		aim_earth(newid);
		Gobjs[newid].hp = 1; //For initial scene
		break;
	case TID_KUMO9_X24_PCANNON:
		//Kumo9's pcanon aims nearest facility
		Gobjs[newid].aiming_target = find_nearest_unit(newid, KUMO9_X24_PCANNON_RANGE, UNITTYPE_FACILITY);
		break;
	}
	return newid;
}

//Add chat message
void chat(char* c) {
	ChatTimeout = CHAT_TIMEOUT;
	//shift out lastest message
	for(uint8_t i = 0; i < MAX_CHAT_COUNT - 1; i++) {
		strcpy(ChatMessages[MAX_CHAT_COUNT - i - 1], ChatMessages[MAX_CHAT_COUNT - i - 2]);
	}
	//Security check
	if(strlen(c) + 1 >= BUFFER_SIZE) {
		die("gamesys.c: chat() failed: message too long.\n");
	}
	strcpy(ChatMessages[0], c);
	g_print("[chat] %s\n", c);
}

void chatf(const char* p, ...) {
	va_list v;
	int32_t r;
	char b[BUFFER_SIZE];
	va_start(v, p);
	r = vsnprintf(b, sizeof(b), p, v);
	va_end(v);
	if(r >= sizeof(b) || r == -1) {
		die("gamesys.c: chatf() failed. Buffer overflow or vsnprintf() failed.\n");
		return;
	}
	chat(b);
}

//calculate distance between Gobjs[id1] and Gobjs[id2]
double get_distance(int32_t id1, int32_t id2){
	if(!is_range(id1, 0, MAX_OBJECT_COUNT - 1) || !is_range(id2, 0, MAX_OBJECT_COUNT - 1)) {
		die("gamesys.c: set_speed_for_following(): bad id passed!\n");
		return INFINITY;
	}
	if(Gobjs[id1].tid == TID_NULL || Gobjs[id2].tid == TID_NULL) {
		return INFINITY;
	}
	return get_distance_raw(Gobjs[id1].x, Gobjs[id1].y, Gobjs[id2].x, Gobjs[id2].y);
}

double get_distance_raw(double x1, double y1, double x2, double y2) {
	double dx = fabs(x1 - x2);
	double dy = fabs(y1 - y2);
	return sqrt((dx * dx) + (dy * dy)); // a*a = b*b + c*c
}

//calculate appropriate speed for following object to Gobjs[srcid]
void set_speed_for_following(int32_t srcid) {
	if(!is_range(srcid, 0, MAX_OBJECT_COUNT - 1)) {
		die("gamesys.c: set_speed_for_following(): bad srcid passed!\n");
		return;
	}
	int32_t targetid = Gobjs[srcid].aiming_target;
	if(!is_range(targetid, 0, MAX_OBJECT_COUNT - 1)) {
		Gobjs[srcid].sx = 0;
		Gobjs[srcid].sy = 0;
		//g_print("gamesys.c: set_speed_for_following(): id %d has bad target id %d.\n", srcid, targetid);
		return;
	}
	if(Gobjs[targetid].tid == TID_NULL) {
		Gobjs[srcid].sx = 0;
		Gobjs[srcid].sy = 0;
		//g_print("gamesys.c: set_speed_for_following(): id %d has target id %d, this object is dead.\n", srcid, targetid);
		return;
	}
	double dstx = Gobjs[targetid].x;
	double dsty = Gobjs[targetid].y;
	LookupResult_t t;
	lookup((uint8_t)Gobjs[srcid].tid, &t);
	double spdlimit = t.maxspeed;
	set_speed_for_going_location(srcid, dstx, dsty, spdlimit);
	//print(f"line 499: ID={srcid} set_speed_for_following maxspeed={spdlimit} sx={sx} sy={sy}")
}

//set appropriate speed for Gobjs[srcid] to go to coordinate dstx, dsty from current coordinate of Gobjs[srcid]
void set_speed_for_going_location(int32_t srcid, double dstx, double dsty, double speedlimit) {
	if(!is_range(srcid, 0, MAX_OBJECT_COUNT - 1)) {
		die("set_speed_for_going_location(): bad srcid passed!\n");
		return;
	}
	double srcx = Gobjs[srcid].x;
	double srcy = Gobjs[srcid].y;
	double dx = fabs(dstx - srcx);
	double dy = fabs(dsty - srcy);
	//If objects are too close, set speed to 0
	if(sqrt( (dx * dx) + (dy * dy) ) < 3) {
		Gobjs[srcid].sx = 0;
		Gobjs[srcid].sy = 0;
		return;
	}
	double sx, sy;
	if(dx > dy) {
		double r = 0;
		if(dy != 0) { r = dy / dx; }
		sx = speedlimit;
		sy = r * speedlimit;
	} else {
		double r = 0;
		if(dx != 0) {r = dx / dy; }
		sy = speedlimit;
		sx = r * speedlimit;
	}
	if(dstx < srcx) { sx = -sx; }
	if(dsty < srcy) { sy = -sy; }
	Gobjs[srcid].sx = sx;
	Gobjs[srcid].sy = sy;
}


//find nearest object (different team) from Gobjs[srcid] within diameter finddist
//if object has same objecttype specified in va_list, they will be excluded
int32_t find_nearest_unit(int32_t srcid, int32_t finddist, facility_type_t cfilter) {
	double mindist = finddist / 2;
	int32_t t = -1;
	if(!is_range(srcid, 0, MAX_OBJECT_COUNT - 1)) {
		die("find_nearest_unit(): bad srcid passed!\n");
		return -1;
	}
	//esrc = self.gobjs[srcid]
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		//e = self.gobjs[i]
		if(Gobjs[i].tid != TID_NULL && srcid != i) {
			//C = self.lookup(e.tid)
			LookupResult_t dstinfo;
			LookupResult_t srcinfo;
			lookup(Gobjs[i].tid, &dstinfo);
			lookup(Gobjs[srcid].tid, &srcinfo);
			//if objects are in same team, exclude from searching
			if(dstinfo.teamid != srcinfo.teamid && (cfilter & dstinfo.objecttype) ) {
				double dist = get_distance(i, srcid);
				//find nearest object
				if(dist < mindist) {
					mindist = dist;
					t = i;
				}
			}
		}
	}
	//if(t != -1){
	//	g_print("gamesys.c: find_nearest_unit(): src=%d id=%d dist=%d\n", srcid, t, mindist);
	//}
	return t;
}

//find random object (different team) from Gobjs[srcid] within diameter finddist
//if object has same objecttype specified in va_list, they will be excluded
int32_t find_random_unit(int32_t srcid, int32_t finddist, facility_type_t cfilter) {
	if(!is_range(srcid, 0, MAX_OBJECT_COUNT - 1)) {
		die("find_nearest_unit(): bad srcid passed!\n");
		return -1;
	}
	uint16_t findobjs[MAX_OBJECT_COUNT];
	uint16_t oc = 0;
	//esrc = self.gobjs[srcid]
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		//e = self.gobjs[i]
		if(Gobjs[i].tid != TID_NULL && srcid != i) {
			//C = self.lookup(e.tid)
			LookupResult_t dstinfo;
			LookupResult_t srcinfo;
			lookup(Gobjs[i].tid, &dstinfo);
			lookup(Gobjs[srcid].tid, &srcinfo);
			//if objects are in same team, exclude from searching
			//if object type is not in cfilter, exclude
			//if object im more far than finddist / 2, exclude
			if(dstinfo.teamid != srcinfo.teamid && (dstinfo.objecttype & cfilter) && get_distance(srcid, i) < finddist / 2) {
				findobjs[oc] = i;
				oc++;
			}
		}
	}
	if(oc == 0) {
		return -1;
	} else {
		uint16_t selnum = (uint16_t)randint(0, oc - 1);
		//if(t != -1){
		//	g_print("gamesys.c: find_nearest_unit(): src=%d id=%d dist=%d\n", srcid, t, mindist);
		//}
		return findobjs[selnum];
	}
}

//Process SMP packet
void process_smp() {
	event_hdr_t *hdr;
	if(RXSMPEventLen > 0) {
		//Process SMP Event
		uint32_t ptr = 0;
		while(ptr < RXSMPEventLen) {
			size_t rem = RXSMPEventLen - ptr;
			//Parse event record header
			if(rem < sizeof(event_hdr_t) ) {
				g_print("process_smp(): Too short event record.\n");
				chat( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
				close_connection_silent();
				return;
			}
			hdr = (event_hdr_t*)&RXSMPEventBuffer[ptr];
			int32_t cid = hdr->cid;
			size_t evlen = ntohs(hdr->evlen);
			size_t reclen = evlen + sizeof(event_hdr_t);
			uint8_t* evhead = &RXSMPEventBuffer[ptr + (int32_t)sizeof(event_hdr_t)];
			if(rem < reclen) {
				g_print("process_smp(): Too short event record.\n");
				chat( (char*)getlocalizedstring(TEXT_SMP_ERROR) );
				close_connection_silent();
				return;
			}
			//Ignore packet from myself
			if(cid != SMPcid) {
				process_smp_events(evhead, evlen, cid);
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
		uint16_t x = (uint16_t)va_arg(varg, double);
		uint16_t y = (uint16_t)va_arg(varg, double);
		ev_placeitem_t ev = {
			.evtype = (uint8_t)etype,
			.tid = tid,
			.x = htons(x),
			.y = htons(y)
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
		uint16_t chatplen = sizeof(ctx);
		ev_chat_t ev = {
			.evtype = etype,
			.clen = htons(chatplen)
		};
		if(is_range(chatplen, 0, NET_CHAT_LIMIT - 1) ) {
			pktlen = sizeof(ev_chat_t) + chatplen;
		} else {
			g_print("net_send_packet(): CHAT: too long message\n");
			pktlen = 0;
		}
		if(TXSMPEventLen + pktlen < SMP_EVENT_BUFFER_SIZE) {
			memcpy(poff, &ev, sizeof(ev_chat_t) );
			memcpy(&poff[sizeof(ev_chat_t)], ctx, chatplen);
		}
	} else if(etype == EV_RESET) {
		//round reset event
		uint32_t level_seed = (uint32_t)va_arg(varg, int);
		uint32_t level_difficulty = (uint32_t)va_arg(varg, int);
		ev_reset_t ev = {
			.evtype = etype,
			.level_seed = htonl(level_seed),
			.level_difficulty = htonl(level_difficulty)
		};
		pktlen = sizeof(ev_reset_t);
		if(TXSMPEventLen + pktlen < SMP_EVENT_BUFFER_SIZE) {
			memcpy(poff, &ev, pktlen);
		}
	} else {
		g_print("net_send_packet(): unknown packet type.\n");
		pktlen = 0;
	}
	va_end(varg);
	if(TXSMPEventLen + pktlen >= SMP_EVENT_BUFFER_SIZE) {
		g_print("net_stack_packet(): TX SMP buffer overflow.\n");
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
				g_print("process_smp_events(): Too short EV_CHANGE_PLAYABLE_SPEED packet, decoder will terminate.\n");
				return;
			}
			ev_changeplayablespeed_t *ev = (ev_changeplayablespeed_t*)&evbuf[evp];
			double sx = (double)ev->sx;
			double sy = (double)ev->sy;
			evp += (int32_t)sizeof(ev_changeplayablespeed_t);
			//Security check
			if(is_range_number(sx, -10, 10) && is_range_number(sy, -10, 10)) {
				//Find playable character that has packet owner's cid
				int32_t objid = -1;
				for(int32_t i = 0; i < MAX_OBJECT_COUNT; i++) {
					if(Gobjs[i].tid != TID_NULL && Gobjs[i].cid == cid && is_playable_character(Gobjs[i].tid) ) {
						Gobjs[i].sx = sx;
						Gobjs[i].sy = sy;
						break;
					}
				}
			} else {
				g_print("process_smp_events(): CHANGE_PLAYABLE_SPEED: packet check failed.\n");
			}
		} else if(evbuf[evp] == EV_PLACE_ITEM) {
			if(remaining < sizeof(ev_placeitem_t) ) {
				g_print("process_smp_events(): Too short EV_PLACE_ITEM packet, decoder will terminate.\n");
				return;
			}
			//ItemPlace on other client
			ev_placeitem_t *ev = (ev_placeitem_t*)&evbuf[evp];
			obj_type_t tid = (obj_type_t)ev->tid;
			double x = (double)ntohs(ev->x);
			double y = (double)ntohs(ev->y);
			evp += (int32_t)sizeof(ev_placeitem_t);
			//Security check
			if(is_range_number(x, 0, MAP_WIDTH) && is_range_number(y, 0, MAP_HEIGHT) && is_range(tid, 0, MAX_TID - 1) ) {
				//Place object and set client id
				g_print("process_smp_events(): PLACE_ITEM: cid=%d, tid=%d, x=%f, y=%f\n", cid, tid, x, y);
				int32_t objid = add_character(tid, x, y, -1);
				Gobjs[objid].cid = cid;
			} else {
				g_print("process_smp_events(): PLACE_ITEM: packet check failed.\n");
			}
		} else if(evbuf[evp] == EV_USE_SKILL) {
			if(remaining < sizeof(ev_useskill_t) ) {
				g_print("process_smp_events(): Too short EV_USE_SKILL packet, decoder will terminate.\n");
				return;
			}
			//UseSkill on other client
			ev_useskill_t *ev = (ev_useskill_t*)&evbuf[evp];
			uint8_t skillid = ev->skillid;
			evp += (int32_t)sizeof(ev_useskill_t);
			//Security check
			if(is_range(skillid, 0, SKILL_COUNT - 1) ) {
				//Implement later
				g_print("process_smp_events(): USESKILL: cid=%d, skillid=%d.\n", cid, skillid);
			} else {
				g_print("process_smp_events(): USESKILL: packet check failed.\n");
			}
		} else if(evbuf[evp] == EV_CHAT) {
			if(remaining < sizeof(ev_chat_t) ) {
				g_print("process_smp_events(): Too short EV_CHAT packet, decoder will terminate.\n");
				return;
			}
			//Chat on other client
			ev_chat_t *ev = (ev_chat_t*)&evbuf[evp];
			char ctx[BUFFER_SIZE];
			ssize_t cpktlen = (ssize_t)ntohs(ev->clen);
			char* netchat = (char*)&evbuf[evp + (int32_t)sizeof(ev_chat_t)];
			if((ssize_t)remaining < (ssize_t)sizeof(ev_chat_t) + cpktlen) {
				g_print("process_smp_events(): Too short EV_CHAT packet, decoder will terminate.\n");
				return;
			}
			evp += (int32_t)sizeof(ev_chat_t) + (int32_t)cpktlen;
			//Security check
			if(is_range( (int32_t)cpktlen, 0, NET_CHAT_LIMIT - 1) ) {
				memcpy(ctx, netchat, (size_t)cpktlen);
				ctx[cpktlen] = 0; //Terminate string with NUL
				chatf("[SMP-CID%d] %s", cid, ctx);
			} else {
				g_print("procss_smp_events(): CHAT: packet too large.\n");
			}
		} else if(evbuf[evp] == EV_RESET) {
			if(remaining < sizeof(ev_reset_t) ) {
				g_print("process_smp_events(): Too short EV_RESET packet, decoder will terminate.\n");
				return;
			}
			ev_reset_t *ev = (ev_reset_t*)&evbuf[evp];
			Difficulty = (int32_t)ntohl(ev->level_difficulty);
			srand(ntohl(ev->level_seed) );
			g_print("process_smp_events(): Round reset request from cid%d\n", cid);
			reset_game();
		} else {
			g_print("process_smp_events(): Unknown event type, decorder terminated.\n");
			break;
		}
	}
}

gboolean gametick(gpointer data) {
	//gametick, called for every 10mS
	//Take care of chat timeout and timers
	if(ChatTimeout != 0) { ChatTimeout--; }
	//if(ErrorShowTimer != 0) { ErrorShowTimer--; }
	//Process subthread message request
	if(SubthreadMessageReq != -1) {
		chat(SubthreadMSGCTX);
		SubthreadMessageReq = -1;
	}
	//SMP Processing
	if(SMPStatus == NETWORK_LOGGEDIN) {
		process_smp();
	}
	//If game is not in playing state, do special process
	switch(GameState) {
	case GAMESTATE_INITROUND:
		//Starting animation: Slowly increase enemy base and the earth hp
		for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
			if(Gobjs[i].tid == TID_EARTH || Gobjs[i].tid == TID_ENEMYBASE) {
				LookupResult_t t;
				lookup((uint8_t)Gobjs[i].tid, &t);
				Gobjs[i].hp = constrain_number(Gobjs[i].hp + 25, 0, t.inithp);
				//If hp became full, move camera to show another object
				if(Gobjs[i].hp == t.inithp) {
					if(Gobjs[i].tid == TID_EARTH) {
						CameraX = MAP_WIDTH - WINDOW_WIDTH;
						CameraY = MAP_HEIGHT - WINDOW_HEIGHT;
					}
					//Enemy HP is much more than earth and this must be second process, if all HP filled, reset camera and change state to playing.
					if(Gobjs[i].tid == TID_ENEMYBASE) {
						CameraX = 0;
						CameraY = 0;
						GameState = GAMESTATE_PLAYING;
					}
				}
			}
		}
		return TRUE;
	case GAMESTATE_GAMECLEAR:
	case GAMESTATE_GAMEOVER:
		//Init game in 5 sec.
		StateChangeTimer++;
		//Pause playable character
		CharacterMove = FALSE;
		if(StateChangeTimer > 500) {
			reset_game();
			return TRUE; //Not doing AI proc after init game, or bug.
		}
		break;
	case GAMESTATE_DEAD:
		//Respawn in 10 sec
		StateChangeTimer--;
		if(StateChangeTimer == 0) {
			spawn_playable();
			GameState = GAMESTATE_PLAYING;
		}
		break;
	}
	if(is_range(PlayingCharacterID, 0, MAX_OBJECT_COUNT - 1) ) {
		proc_playable_op();
	}
	//Care about cooldown timer
	for(uint8_t i = 0; i < ITEM_COUNT; i++) {
		if(ItemCooldownTimers[i] != 0) {ItemCooldownTimers[i]--;}
	}
	for(uint8_t i = 0; i < SKILL_COUNT; i++) {
		if(SkillCooldownTimers[i] != 0) {SkillCooldownTimers[i]--;}
	}
	procai();
	return !ProgramExiting;
}

void proc_playable_op() {
	if(!is_range(PlayingCharacterID, 0, MAX_OBJECT_COUNT - 1)) {
		die("proc_playable_op(): Playable character ID is wrong.\n");
		return;
	}
	if(Gobjs[PlayingCharacterID].tid == -1) {
		g_print("proc_playable_op(): Player TID is -1??\n");
		return;
	}
	PlayableInfo_t plinf;
	lookup_playable(CurrentPlayableCharacterID, &plinf);
	//Move Playable character
	double tx = 0, ty = 0;
	static double prevtx = 0, prevty = 0;
	if(CharacterMove && GameState == GAMESTATE_PLAYING) {
		//If CharacterMove == TRUE, and PALYING state, playable character will follow mouse
		//Set player move speed
		double cx, cy;
		double playerspd = 1.0 + (MapTechnologyLevel * 0.5);
		getlocalcoord(PlayingCharacterID, &cx, &cy);
		double dx, dy;
		dx = constrain_number(CursorX, cx - 100, cx + 100) - (cx - 100);
		dy = constrain_number(CursorY, cy - 100, cy + 100) - (cy - 100);
		//g_print("%f, %f, %f, %f\n", dx, dy, cx, cy);
		tx = scale_number(dx, 200, playerspd * 2) - playerspd;
		ty = playerspd - scale_number(dy, 200, playerspd * 2);
	}
	Gobjs[PlayingCharacterID].sx = tx;
	Gobjs[PlayingCharacterID].sy = ty;
	if(prevtx != tx || prevty != ty) {
		//Speed changed
		if(SMPStatus == NETWORK_LOGGEDIN) {
			stack_packet(EV_CHANGE_PLAYABLE_SPEED, tx, ty);
		}
		prevtx = tx;
		prevty = ty;
	}
	//Set camera location to display playable character in the center of display.
	CameraX = (int32_t)constrain_number(Gobjs[PlayingCharacterID].x - (WINDOW_WIDTH / 2.0), 0, MAP_WIDTH - WINDOW_WIDTH);
	CameraY = (int32_t)constrain_number(Gobjs[PlayingCharacterID].y - (WINDOW_HEIGHT / 2.0), 0, MAP_HEIGHT - WINDOW_HEIGHT);
	//Process skill keys if playing
	if(GameState == GAMESTATE_PLAYING) {
		const keyflags_t SKILLKFLG[SKILL_COUNT] = {KEY_F1, KEY_F2, KEY_F3};
		for(uint8_t i = 0; i < SKILL_COUNT; i++) {
			if(KeyFlags & SKILLKFLG[i]) {
				//KeyPress
				//Remember pressed skill key if cooldown is 0 and no other skill key pressed
				if(SkillKeyState == -1 && SkillCooldownTimers[i] == 0) {SkillKeyState = i;}
				//if(SkillCooldownTimers[i] != 0) {chat(getlocalizedstring(10));}
			} else {
				//KeyRelease
				if(SkillKeyState == i) {
					//Dedicated key pressed before
					SkillKeyState = -1;
					//Activate Skill
					Gobjs[PlayingCharacterID].timers[i + 1] = plinf.skillinittimers[i];
					if(SMPStatus == NETWORK_LOGGEDIN) {
						stack_packet(EV_USE_SKILL, i); //if logged in to SMP server, notify event instead
					}
					//Reset Skill CD
					if(DebugMode) {
						SkillCooldownTimers[i] = 100;
					} else {
						SkillCooldownTimers[i] = plinf.skillcooldowns[i];
					}
				}
			}
		}
	}
}

void use_item() {
	if(GameState != GAMESTATE_PLAYING) {
		return;
	}
	//SelectingItemID boundary check
	if(SelectingItemID == -1) {
		return;
	}
	if(!is_range(SelectingItemID, 0, ITEM_COUNT - 1) ) {
		die("use_item(): Bad SelectingItemID!\n");
		return;
	}
	//Check for price
	if(ITEMPRICES[SelectingItemID] > Money && !DebugMode) {
		//chat(getlocalizedstring(10) );
		return;
	}
	//Check for cooldown
	if(ItemCooldownTimers[SelectingItemID] != 0 && !DebugMode) {
		//chat(getlocalizedstring(10) );
		return;
	}
	//Buy facility
	if( buy_facility((uint8_t)SelectingItemID) == FALSE) {
		return; //Could not buy facility
	}
	//If succeed, decrease money and set up Cooldown timer
	if(!DebugMode) {
		Money -= ITEMPRICES[SelectingItemID];
		ItemCooldownTimers[SelectingItemID] = ITEMCOOLDOWNS[SelectingItemID];
	}
}

gboolean buy_facility(int32_t fid) {
	//Try to buy facility (Key handler)
	obj_type_t tid = FTIDS[fid];
	if(tid == TID_NULL) {
		die("buy_facility(): This tid is not facility!.\n");
		return FALSE;
	}
	double mx, my;
	local2map(CursorX, CursorY, &mx, &my);
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		if(Gobjs[i].tid == TID_NULL) { continue; }
		LookupResult_t t;
		lookup(Gobjs[i].tid, &t);
		if(t.objecttype == UNITTYPE_FACILITY) {
			if(get_distance_raw(Gobjs[i].x, Gobjs[i].y, mx, my) < 500) {
				//showerrorstr(0);
				chat( (char*)getlocalizedstring(0));
				return FALSE;
			}
		}
	}
	add_character(tid, mx, my, OBJID_INVALID);
	//If connected to remote server, notify item place.
	if(SMPStatus == NETWORK_LOGGEDIN) {
		stack_packet(EV_PLACE_ITEM, tid, mx, my);
	}
	return TRUE;
}

//Select next item candidate
void select_next_item() {
	if(SelectingItemID < ITEM_COUNT - 1) {
		SelectingItemID++;
	} else {
		SelectingItemID = -1;
	}
}

//Select previous item candidate
void select_prev_item() {
	if(SelectingItemID > -1) {
		SelectingItemID--;
	} else {
		SelectingItemID = ITEM_COUNT - 1;
	}
}

void debug_add_character() {
	if(DebugMode) {
		double mx, my;
		local2map(CursorX, CursorY, &mx, &my);
		add_character(AddingTID, mx, my, OBJID_INVALID);
		stack_packet(EV_PLACE_ITEM, mx, my);
	}
}

void start_command_mode(gboolean c) {
	KeyFlags = 0;
	SkillKeyState = -1;
	if(c) {
		strcpy(CommandBuffer, "/");
		CommandCursor = 1;
	} else {
		CommandBuffer[0] = 0;
		CommandCursor = 0;
	}
}

void switch_character_move() {
	//M key handler
	if(GameState == GAMESTATE_PLAYING) {
		if(CharacterMove) {
			CharacterMove = FALSE;
		} else {
			CharacterMove = TRUE;
		}
	}
}

void execcmd() {
	//Command Handler: called when command entered
	if(strcmp(CommandBuffer, "/version") == 0) {
		//Show Version
		chatf("Version: %s", VERSION_STRING);
	} else if(strcmp(CommandBuffer, "/credit") == 0) {
		//Show Credit
		chat(CREDIT_STRING);
	} else if(strcmp(CommandBuffer, "/debugon") == 0) {
		//Debug mode on
		DebugMode = TRUE;
	} else if(strcmp(CommandBuffer, "/debugoff") == 0) {
		//Debug off
		DebugMode = FALSE;
	} else if(memcmp(CommandBuffer, "/addid ", 7) == 0) {
		//Set appending object id (for debug)
		int32_t i = (int32_t)strtol(&CommandBuffer[7], NULL, 10);
		if(is_range(i, 0, MAX_TID - 1) ) {
			AddingTID = i;
		} else {
			chat( (char*)getlocalizedstring(TEXT_BAD_COMMAND_PARAM) ); //Bad parameter
		}
	} else if(memcmp(CommandBuffer, "/difficulty ", 12) == 0) {
		//Changing difficulty
		int32_t i = (int32_t)strtol(&CommandBuffer[12], NULL, 10);
		if(is_range(i, 1, MAX_DIFFICULTY) ) {
			Difficulty = i;
		} else {
			chat( (char*)getlocalizedstring(TEXT_BAD_COMMAND_PARAM) ); //Bad parameter
		}
	} else if(memcmp(CommandBuffer, "/smp ", 5) == 0) {
		//Changing selecting smp prof id (Can't be changed in SMP)
		if(SMPStatus != NETWORK_DISCONNECTED) {
			g_print("execcmd(): /smp: in connection, you can not change SMP profile.\n");
			chat( (char*)getlocalizedstring(TEXT_UNAVAILABLE) ); //Unavailable
			return;
		}
		int32_t i = (int32_t)strtol(&CommandBuffer[5], NULL, 10);
		if(is_range(i, 1, SMPProfCount) ) {
			SelectedSMPProf = i - 1;
		} else {
			chat( (char*)getlocalizedstring(TEXT_BAD_COMMAND_PARAM) ); //Bad parameter
		}
	} else if(strcmp(CommandBuffer, "/getsmps") == 0) {
		//Get current loaded SMP profile count
		chatf("getsmps: %d", SMPProfCount);
	} else if(strcmp(CommandBuffer, "/getsmp") == 0) {
		//Get current selected SMP profile
		if(!is_range(SelectedSMPProf, 0, SMPProfCount) ) {
			chatf("getsmp: %d (ID overflow)", SelectedSMPProf + 1);
		} else {
			chatf("getsmp: %d (%s@%s:%s)", SelectedSMPProf + 1, SMPProfs[SelectedSMPProf].usr, SMPProfs[SelectedSMPProf].host, SMPProfs[SelectedSMPProf].port);
		}
	} else if(strcmp(CommandBuffer, "/reset") == 0) {
		//Reset game round
		if(SMPStatus == NETWORK_LOGGEDIN) {
			stack_packet(EV_RESET);
		} else {
			reset_game();
		}
	} else if(strcmp(CommandBuffer, "/jp") == 0) {
		//Change language to Japanese
		LangID = LANGID_JP;
	} else if(strcmp(CommandBuffer, "/en") == 0) {
		//Change language to English
		LangID = LANGID_EN;
	} else if(memcmp(CommandBuffer, "/chfont ", 8) == 0) {
		//Load font list
		loadfont(&CommandBuffer[8]);
	} else if(strcmp(CommandBuffer, "/connect") == 0) {
		//Connect to SMP server
		connect_server();
	} else if(strcmp(CommandBuffer, "/disconnect") == 0) {
		//Disconnect from SMP server
		close_connection(0);
	} else {
		//If not command, treat them as a chat
		chat(CommandBuffer);
		if(SMPStatus == NETWORK_LOGGEDIN) {
			stack_packet(EV_CHAT, CommandBuffer);
		}
	}
}

void commandmode_keyhandler(guint keyval, GdkModifierType state) {
	//command mode
	if(keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
		//Enter
		CommandCursor = -1;
		//g_print("Command: %s\n", CommandBuffer);
		execcmd(CommandBuffer);
	} else if(keyval == GDK_KEY_BackSpace) {
		//BackSpace
		if(CommandCursor > 0 && !CommandBufferMutex) {
			CommandBufferMutex = TRUE;
			char *s = g_utf8_offset_to_pointer(CommandBuffer, CommandCursor - 1); //String after Deletion
			char *s2 = g_utf8_offset_to_pointer(CommandBuffer, CommandCursor); //String after current cursor
			//Shift left string
			for(uint16_t i = 0; i < strlen(s) + 1; i++) {
				s[i] = s2[i];
			}
			CommandCursor--;
			CommandBufferMutex = FALSE;
		}
	} else if(keyval == GDK_KEY_Left) {
		//LeftArrow
		if(CommandCursor > 0) {CommandCursor--;}
	} else if(keyval == GDK_KEY_Right) {
		//RightArrow
		if(CommandCursor < g_utf8_strlen(CommandBuffer, 65535) ) {
			CommandCursor++;
		}
	} else if(keyval == GDK_KEY_v && (state & GDK_CONTROL_MASK) != 0) {
		//CtrlV
		gdk_clipboard_read_text_async(GClipBoard, NULL, clipboard_read_handler, NULL);
	} else if(keyval == GDK_KEY_Escape) {
		//Escape
		CommandCursor = -1;
	} else {
		//Others
		uint32_t t = gdk_keyval_to_unicode(keyval);
		//g_print("%d\n", keycode);
		if(is_range((int32_t)t, 0x20, 0x7e) && !CommandBufferMutex) {
			CommandBufferMutex = TRUE;
			char s[] = {(char)t, 0};
			if(strlen(CommandBuffer) + strlen(s) + 1 <= sizeof(CommandBuffer) ) {
				utf8_insertstring(CommandBuffer, s, CommandCursor, sizeof(CommandBuffer) );
				CommandCursor += 1;
			} else {
				g_print("main.c: commandmode_keyhandler(): Append letter failed: Buffer full.\n");
			}
			CommandBufferMutex = FALSE;
		}
	}
}

void clipboard_read_handler(GObject* obj, GAsyncResult* res, gpointer data) {
	//Data type check
	const GdkContentFormats* f = gdk_clipboard_get_formats(GClipBoard);
	gsize i;
	const GType* t = gdk_content_formats_get_gtypes(f, &i);
	if(t == NULL) {
		g_print("main.c: clipboard_read_handler(): gdk_content_formats_get_gtypes() failed.\n");
		return;
	}
	if(i != 1 || t[0] != G_TYPE_STRING) {
		g_print("main.c: clipboard_read_handler(): Data type missmatch.\n");
		return;
	}
	//Get text and insert into CommandBuffer
	char *cb = gdk_clipboard_read_text_finish(GClipBoard, res, NULL);
	//g_print("Clipboard string size: %d\nCommandBuffer length: %d\n", l, (uint32_t)strlen(CommandBuffer));
	CommandBufferMutex = TRUE;
	if(utf8_insertstring(CommandBuffer, cb, CommandCursor, sizeof(CommandBuffer) ) == TRUE) {
		CommandCursor += (int32_t)g_utf8_strlen(cb, 65535);
	} else {
		g_print("main.c: clipboard_read_handler(): insert failed.\n");
	}
	CommandBufferMutex = FALSE;
	free(cb);
}

void reset_game() {
	const double START_POS = 200;
	GameState = GAMESTATE_INITROUND;
	MapTechnologyLevel = 0;
	MapEnergyLevel = 0;
	Money = 0;
	SelectingItemID = -1;
	CameraX = 0;
	CameraY = 0;
	SkillKeyState = -1;
	//Init Skill state and timer
	for(uint8_t i = 0; i < SKILL_COUNT; i++) {
		SkillCooldownTimers[i] = 0;
	}
	//Initialize Cooldown Timer
	for(uint8_t i = 0; i < ITEM_COUNT; i++) {
		ItemCooldownTimers[i] = 0;
	}
	//Initialize GameObj Slots
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		Gobjs[i].tid = TID_NULL;
	}
	EarthID = add_character(TID_EARTH, START_POS, START_POS, OBJID_INVALID);
	//Place enemy zundamon basee according to difficulty,
	//Everytime difficulty increases, the base is placed in top left, bottom left, topright corner
	//If difficulty is over or equal 4, placing starts on top left corner again, but they must
	//have enough distance from existing enemy base.
	for(int32_t i = 1; i <= Difficulty; i++) {
		const double EDGE_X = MAP_WIDTH - 200;
		const double EDGE_Y = MAP_HEIGHT - 200;
		double X[MAX_DIFFICULTY] = {EDGE_X, EDGE_X, START_POS, EDGE_X - 500, EDGE_X - 500};
		double Y[MAX_DIFFICULTY] = {EDGE_Y, START_POS, EDGE_Y, EDGE_Y, START_POS};
		add_character(TID_ENEMYBASE, X[i - 1], Y[i - 1], OBJID_INVALID);
	}
	spawn_playable();
}

//Spawn playable character
void spawn_playable() {
	PlayableInfo_t t;
	lookup_playable(CurrentPlayableCharacterID, &t);
	CharacterMove = FALSE;
	double x = WINDOW_WIDTH / 2, y = WINDOW_HEIGHT / 2;
	PlayingCharacterID = add_character(t.associatedtid, x, y, OBJID_INVALID);
	if(SMPStatus == NETWORK_LOGGEDIN) {
		stack_packet(EV_PLACE_ITEM, t.associatedtid, x, y);
	}
}

//Set object to aim the earth
void aim_earth(int32_t i) {
	//Gobjs[i].aiming_target = (int16_t)find_earth();
		for(uint16_t j = 0; j < MAX_OBJECT_COUNT; j++) {
			if(Gobjs[j].tid == TID_EARTH) {
				Gobjs[i].aiming_target = j;
				break;
			}
		}
	set_speed_for_following(i);
}
/*
void showerrorstr(int32_t errorid) {
	RecentErrorId = errorid;
	ErrorShowTimer = ERROR_SHOW_TIMEOUT;
}
*/
