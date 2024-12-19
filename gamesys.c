/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

gamesys.c: game process and related functions
*/

#include "main.h"

#include <string.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>

//#include <pango/pangocairo.h>
#include <cairo/cairo.h>

cairo_surface_t *Gsfc = NULL; //GameScreen surface
cairo_t* G = NULL; //Gamescreen cairo context
cairo_surface_t *Plimgs[IMAGE_COUNT]; //Preloaded images
extern const char* IMGPATHES[]; //preload image pathes
SMPProfile_t* t_SMPProf = NULL;
char ChatMessages[MAX_CHAT_COUNT][BUFFER_SIZE];
int32_t ChatTimeout = 0;
GameObjs_t Gobjs[MAX_OBJECT_COUNT];
int32_t CameraX = 0, CameraY = 0;
int32_t CursorX, CursorY;
obj_type_t AddingTID;
int32_t CommandCursor = -1; //Command System Related
char CommandBuffer[BUFFER_SIZE];
int32_t DebugMode = 0;
gamestate_t GameState;
int32_t StateChangeTimer = 0;
int32_t PlayingCharacterID = -1;
int32_t Money;
int32_t EarthID;
int32_t CharacterMove;
int32_t SelectingItemID;
extern const int32_t ITEMPRICES[ITEM_COUNT];
extern const int32_t FTIDS[ITEM_COUNT];
langid_t LangID;
int32_t ItemCooldownTimers[ITEM_COUNT];
extern int32_t ITEMCOOLDOWNS[ITEM_COUNT];
int32_t SkillCooldownTimers[SKILL_COUNT];
int32_t CurrentPlayableCharacterID = 0;
keyflags_t KeyFlags;
int32_t ProgramExiting = 0;
int32_t MapTechnologyLevel;
int32_t MapEnergyLevel;
int32_t MapRequiredEnergyLevel = 0;
int32_t SkillKeyState;
int32_t SubthreadMessageReq = -1;
char SubthreadMSGCTX[BUFFER_SIZE];
smpstatus_t SMPStatus = NETWORK_DISCONNECTED;
int32_t Difficulty = 1;
int32_t SMPProfCount = 0;
SMPProfile_t *SMPProfs = NULL;
int32_t SelectedSMPProf = 0;
SMPPlayers_t SMPPlayerInfo[MAX_CLIENTS];
extern int32_t SMPcid;
int32_t StatusShowTimer;
char StatusTextBuffer[BUFFER_SIZE];
int32_t CommandBufferMutex = 0;
//PangoLayout *PangoL = NULL;

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
		if(r != OBJID_INVALID) {
			Gobjs[newid].aiming_target = r;
			set_speed_for_following(newid);
		} else {
			Gobjs[newid].tid = TID_NULL;
			printf("add_character(): object destroyed, no target found.\n");
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
	printf("[chat] %s\n", c);
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
//returns OBJID_INVALID if not found
int32_t find_nearest_unit(int32_t srcid, int32_t finddist, facility_type_t cfilter) {
	double mindist = finddist / 2;
	int32_t t = OBJID_INVALID;
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
//returns OBJID_INVALID if not found
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
		return OBJID_INVALID;
	} else {
		uint16_t selnum = (uint16_t)randint(0, oc - 1);
		//if(t != -1){
		//	g_print("gamesys.c: find_nearest_unit(): src=%d id=%d dist=%d\n", srcid, t, mindist);
		//}
		return findobjs[selnum];
	}
}

void gametick() {
	poll_socket();
	//gametick, called for every 10mS
	//Take care of chat timeout and timers
	if(ChatTimeout != 0) { ChatTimeout--; }
	if(StatusShowTimer != 0) { StatusShowTimer--; }
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
		return;
	case GAMESTATE_GAMECLEAR:
	case GAMESTATE_GAMEOVER:
		//Init game in 5 sec.
		StateChangeTimer++;
		//Pause playable character
		CharacterMove = 0;
		if(StateChangeTimer > 500) {
			reset_game();
			return; //Not doing AI proc after init game, or bug.
		}
		break;
	case GAMESTATE_DEAD:
		//Respawn in 10 sec
		StateChangeTimer--;
		if(StateChangeTimer == 0) {
			spawn_playable_me();
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
}

void proc_playable_op() {
	if(!is_range(PlayingCharacterID, 0, MAX_OBJECT_COUNT - 1)) {
		die("proc_playable_op(): Playable character ID is wrong.\n");
		return;
	}
	if(Gobjs[PlayingCharacterID].tid == -1) {
		printf("proc_playable_op(): Player TID is -1??\n");
		return;
	}
	PlayableInfo_t plinf;
	lookup_playable(CurrentPlayableCharacterID, &plinf);
	//Move Playable character
	double tx = 0, ty = 0;
	static double prevtx = 0, prevty = 0;
	if(CharacterMove && GameState == GAMESTATE_PLAYING) {
		//If CharacterMove == 1, and PALYING state, playable character will follow mouse
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
					use_skill(PlayingCharacterID, i, plinf);
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

//Activate Skill
void use_skill(int32_t cid, int32_t sid, PlayableInfo_t plinf) {
	if(!is_range(cid, 0, MAX_OBJECT_COUNT - 1) || !is_range(sid, 0, SKILL_COUNT - 1) || is_playable_character(Gobjs[cid].tid) == 0) {
		die("use_skill(): bad parameter!\n");
		return;
	}
	Gobjs[cid].timers[sid + 1] = plinf.skillinittimers[sid];
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
	if( buy_facility((uint8_t)SelectingItemID) != 0) {
		return; //Could not buy facility
	}
	//If succeed, decrease money and set up Cooldown timer
	if(!DebugMode) {
		Money -= ITEMPRICES[SelectingItemID];
		ItemCooldownTimers[SelectingItemID] = ITEMCOOLDOWNS[SelectingItemID];
	}
}

int32_t buy_facility(int32_t fid) {
	//Try to buy facility (Key handler)
	obj_type_t tid = FTIDS[fid];
	if(tid == TID_NULL) {
		die("buy_facility(): This tid is not facility!.\n");
		return 1;
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
				return 1;
			}
		}
	}
	add_character(tid, mx, my, OBJID_INVALID);
	//If connected to remote server, notify item place.
	if(SMPStatus == NETWORK_LOGGEDIN) {
		stack_packet(EV_PLACE_ITEM, tid, mx, my);
	}
	return 0;
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

void start_command_mode(int32_t c) {
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
			CharacterMove = 0;
		} else {
			CharacterMove = 1;
		}
	}
}

void execcmd() {
	//Command Handler: called when command entered
	if(strlen(CommandBuffer) == 0) {
		return;
	}
	if(strcmp(CommandBuffer, "/version") == 0) {
		//Show Version
		chatf("Version: %s", VERSION_STRING);
	} else if(strcmp(CommandBuffer, "/credit") == 0) {
		//Show Credit
		chat(CREDIT_STRING);
	} else if(strcmp(CommandBuffer, "/debugon") == 0) {
		//Debug mode on
		DebugMode = 1;
	} else if(strcmp(CommandBuffer, "/debugoff") == 0) {
		//Debug off
		DebugMode = 0;
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
			printf("execcmd(): /smp: in connection, you can not change SMP profile.\n");
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
		if(!is_range(SelectedSMPProf, 0, SMPProfCount - 1) ) {
			chatf("getsmp: %d (ID overflow)", SelectedSMPProf + 1);
		} else {
			chatf("getsmp: %d (%s@%s:%s)", SelectedSMPProf + 1, SMPProfs[SelectedSMPProf].usr, SMPProfs[SelectedSMPProf].host, SMPProfs[SelectedSMPProf].port);
		}
	} else if(strcmp(CommandBuffer, "/reset") == 0) {
		//Reset game round
		if(SMPStatus != NETWORK_LOGGEDIN) {
			reset_game();
		} else {
			stack_packet(EV_RESET);
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
		if(SMPStatus == NETWORK_LOGGEDIN) {
			stack_packet(EV_CHAT, CommandBuffer);
		} else {
			chatf("[local] %s", CommandBuffer);
		}
	}
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
	spawn_playable_me(); //Spawn local character
	//Spawn SMP remote playable character
	if(SMPStatus == NETWORK_LOGGEDIN) {
		for(int32_t i = 0; i < MAX_CLIENTS; i++) {
			int32_t cid = SMPPlayerInfo[i].cid;
			if(cid != -1 && cid != SMPcid) {
				//If not empty record nor myself
				int32_t t = spawn_playable(SMPPlayerInfo[i].pid);
				SMPPlayerInfo[i].playable_objid = t;
			}
		}
	}
}

//Spawn playable character
void spawn_playable_me() {
	CharacterMove = 0;
	PlayingCharacterID = spawn_playable(CurrentPlayableCharacterID);
	//For SMP, register own playable character object id to client list
	if(SMPStatus == NETWORK_LOGGEDIN) {
		int32_t i = lookup_smp_player_from_cid(SMPcid);
		if(is_range(i, 0, MAX_CLIENTS - 1) ) {
			SMPPlayerInfo[i].playable_objid = PlayingCharacterID;
		} else {
			printf("spawn_playable(): Could not register my playable character id to list.\n");
		}
	}
}

//Spawn specificated playable character by pid at fixed coordinate
int32_t spawn_playable(int32_t pid) {
	PlayableInfo_t t;
	lookup_playable(pid, &t);
	return add_character(t.associatedtid, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, OBJID_INVALID);
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

void showstatus(const char* ctx, ...) {
	va_list varg;
	va_start(varg, ctx);
	vsnprintf(StatusTextBuffer, BUFFER_SIZE, ctx, varg);
	va_end(varg);
	StatusTextBuffer[BUFFER_SIZE - 1] = 0; //For Additional Security
	StatusShowTimer = ERROR_SHOW_TIMEOUT;
}

int32_t gameinit() {
	//Gameinit function, load assets and more.
	read_creds(); //read smp profiles
	
	//Create game screen and its content
	Gsfc = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WINDOW_WIDTH, WINDOW_HEIGHT);
	G = cairo_create(Gsfc);
	if(cairo_status(G) != CAIRO_STATUS_SUCCESS) {
		printf("gameinit(): Making game screen buffer failed.\n");
		do_finalize();
		return -1;
	}

	//Load images into memory.
	printf("Loading image assets.\n");
	for(int32_t i = 0; i < IMAGE_COUNT; i++) {
		Plimgs[i] = NULL;
	}
	for(int32_t i = 0; i < IMAGE_COUNT; i++) {
		Plimgs[i] = cairo_image_surface_create_from_png(IMGPATHES[i]);
		if(cairo_surface_status(Plimgs[i]) != CAIRO_STATUS_SUCCESS) {
			printf("gameinit(): Error loading %s\n", IMGPATHES[i]);
			return -1;
		}
	}
	
	detect_syslang();
	
	//Initialize Chat Message Slots
	for(uint8_t i = 0; i < MAX_CHAT_COUNT; i++) {
		ChatMessages[i][0] = 0;
	}
	
	check_data(); //Data Check

	//PangoL = pango_cairo_create_layout(G);
	
	//Setup font
	loadfont("Ubuntu Mono,monospace");
	set_font_size(FONT_DEFAULT_SIZE);
	
	reset_game(); //reset game round
	return 0;
}

void do_finalize() {
	printf("do_finalize(): bye. thank you for playing.\n");
	//Finalize
	ProgramExiting = 1; //Notify that program is exiting
	
	//Clean SMP profiles
	if(SMPProfs != NULL) {
		free(SMPProfs);
	}
	
	//Close Network Connection
	if(SMPStatus != NETWORK_DISCONNECTED) {
		close_tcp_socket();
	}
	
	//Unload images
	for(uint8_t i = 0; i < IMAGE_COUNT; i++) {
		if(Plimgs[i] != NULL) {
			cairo_surface_destroy(Plimgs[i]);
		}
	}
	
	//Unload other resources
	//if(PangoL != NULL) {
	//	g_object_unref(PangoL);
	//}
	if(G != NULL) {
		cairo_destroy(G);
	}
	if(Gsfc != NULL) {
		cairo_surface_destroy(Gsfc);
	}
}

//Read SMP Credentials from file
void read_creds() {
	FILE* f = fopen("credentials.txt", "r");
	char buf[BUFFER_SIZE];
	if(f == NULL) {
		printf("Can not open credentials.txt: %s\n", strerror(errno) );
		return;
	}
	int lineno = 0;
	while(fgets(buf, BUFFER_SIZE, f) != NULL) {
		buf[BUFFER_SIZE - 1] = 0; //For Additional security
		lineno++;
		char *t = buf;
		int32_t err = 0;
		SMPProfile_t t_rec;
		for(int32_t i = 0; i < 4; i++) {
			const size_t rec_size[] = {HOSTNAME_SIZE, PORTNAME_SIZE, UNAME_SIZE, PASSWD_SIZE};
			char *rec_ptr[] = {t_rec.host, t_rec.port, t_rec.usr, t_rec.pwd};
			//Find splitter letter
			char *t2 = strchr(t, '\t');
			//Last record will be finished by newline character
			if(i == 3) {
				t2 = strchr(t, '\n');
			}
			if(t2 == NULL) {
				printf("read_creds(): credentials.txt:%d: No splitter letter, parsing skipped for the line.\n", lineno);
				err = 1;
				break;
			}
			//Measure distance to the letter
			size_t reclen = (size_t)t2 - (size_t)t;
			if(reclen >= rec_size[i]) {
				printf("read_creds(): credentials.txt:%d: Record size overflow, persing skipped for the line.\n", lineno);
				err = 1;
				break;
			}
			if(reclen == 0) {
				printf("read_creds(): credentials.txt:%d: Empty record detected, parsing skipped for this line.\n", lineno);
				err = 1;
				break;
			}
			//Copy string from t to the splitter letter into elem
			memcpy(rec_ptr[i], t, reclen);
			rec_ptr[i][reclen] = 0;
			t = &t2[1]; //Set next record finding position right after the splitter letter
		}
		if(err) {
			continue;
		}
		printf("SMP Credential %d: Host: %s, Port: %s, User: %s\n", SMPProfCount, t_rec.host, t_rec.port, t_rec.usr);
		//If no error occurs, allocate memory for new record and copy.
		if(SMPProfs == NULL) {
			SMPProfs = malloc(sizeof(SMPProfile_t) );
			if(SMPProfs == NULL) {
				printf("read_creds(): insufficient memory, malloc failure.\n");
				return;
			}
		} else {
			t_SMPProf = realloc(SMPProfs, sizeof(SMPProfile_t) * (size_t)(SMPProfCount + 1) );
			if(t_SMPProf == NULL) {
				printf("read_creds(): insufficient memory, realloc failure.\n");
				free(SMPProfs);
				SMPProfs = NULL;
				return;
			}
			SMPProfs = t_SMPProf;
		}
		memcpy(&SMPProfs[SMPProfCount], &t_rec, sizeof(SMPProfile_t) );
		SMPProfCount++;
	}
	fclose(f);
}

void mousemotion_handler(int32_t x, int32_t y) {
	//Called when mouse moved, save mouse location value
	CursorX = (int32_t)x;
	CursorY = (int32_t)y;
}

void keypress_handler(char kc, specialkey_t ks) {
	if(CommandCursor == -1) {
		//normal mode
		switch(kc) {
		case 't':
		case 'T':
			//T
			start_command_mode(0);
			break;
		case '/':
			// slash
			start_command_mode(1);
			break;
		case ' ':
			//space
			switch_character_move();
			break;
		case 'a':
		case 'A':
			//A
			select_prev_item();
			break;
		case 's':
		case 'S':
			//S
			select_next_item();
			break;
		case 'd':
		case 'D':
			//D
			use_item();
			break;
		case 'f':
		case 'F':
			//F (Debug Key)
			debug_add_character();
			break;
		case 'q':
		case 'Q':
			//Q
			if( (KeyFlags & KEY_F1) == 0) { KeyFlags += KEY_F1; }
			break;
		case 'w':
		case 'W':
			//W
			if( (KeyFlags & KEY_F2) == 0) { KeyFlags += KEY_F2; }
			break;
		case 'e':
		case 'E':
			//E
			if( (KeyFlags & KEY_F3) == 0) { KeyFlags += KEY_F3; }
			break;
		case 'j':
		case 'J':
			//J
			if( (KeyFlags & KEY_LEFT) == 0) { KeyFlags += KEY_LEFT; }
			break;
		case 'k':
		case 'K':
			//K
			if( (KeyFlags & KEY_DOWN) == 0) { KeyFlags += KEY_DOWN; }
			break;
		case 'l':
		case 'L':
			//L
			if( (KeyFlags & KEY_RIGHT) == 0) { KeyFlags += KEY_RIGHT; }
			break;
		case 'i':
		case 'I':
			//I
			if( (KeyFlags & KEY_UP) == 0) { KeyFlags += KEY_UP; }
			break;
		}
	} else {
		//command mode
		//printf("%02d\n", kc);
		if(ks == SPK_ENTER) {
			//Enter
			CommandCursor = -1;
			execcmd(CommandBuffer);
		} else if(ks == SPK_BS) {
			//BackSpace
			if(CommandCursor > 0 && !CommandBufferMutex) {
				CommandBufferMutex = 1;
				char *s = utf8_strlen_to_pointer(CommandBuffer, CommandCursor - 1); //String after Deletion
				char *s2 = utf8_strlen_to_pointer(CommandBuffer, CommandCursor); //String after current cursor
				//Shift left string
				for(uint16_t i = 0; i < strlen(s) + 1; i++) {
					s[i] = s2[i];
				}
				CommandCursor--;
				CommandBufferMutex = 0;
			}
		} else if(ks == SPK_LEFT) {
			//LeftArrow
			if(CommandCursor > 0) {CommandCursor--;}
		} else if(ks == SPK_RIGHT) {
			//RightArrow
			if(CommandCursor < utf8_strlen(CommandBuffer) ) {
				CommandCursor++;
			}
		} else if(kc == 0x16) {
			//CtrlV
			//gdk_clipboard_read_text_async(GClipBoard, NULL, clipboard_read_handler, NULL);
		} else if(ks == SPK_ESC) {
			//Escape
			CommandCursor = -1;
		} else {
			//Others
			//printf("%d\n", keycode);
			if(is_range(kc, 0x20, 0x7e) && !CommandBufferMutex) {
				CommandBufferMutex = 1;
				char s[] = {kc, 0};
				if(strlen(CommandBuffer) + strlen(s) + 1 <= sizeof(CommandBuffer) ) {
					utf8_insertstring(CommandBuffer, s, CommandCursor, sizeof(CommandBuffer) );
					CommandCursor += 1;
				} else {
					printf("commandmode_keyhandler(): Append letter failed: Buffer full.\n");
				}
				CommandBufferMutex = 0;
			}
		}
	}
}

void keyrelease_handler(char kc) {
	if(CommandCursor == -1) {
		//Operate keyflags on command mode
		switch(kc) {
		case 'q':
		case 'Q':
			//Q
			if(KeyFlags & KEY_F1) { KeyFlags -= KEY_F1; }
			break;
		case 'w':
		case 'W':
			//W
			if(KeyFlags & KEY_F2) { KeyFlags -= KEY_F2; }
			break;
		case 'e':
		case 'E':
			//E
			if(KeyFlags & KEY_F3) { KeyFlags -= KEY_F3; }
			break;
		case 'j':
		case 'J':
			//J
			if(KeyFlags & KEY_LEFT) { KeyFlags -= KEY_LEFT; }
			break;
		case 'k':
		case 'K':
			//K
			if(KeyFlags & KEY_DOWN) { KeyFlags -= KEY_DOWN; }
			break;
		case 'l':
		case 'L':
			//L
			if(KeyFlags & KEY_RIGHT) { KeyFlags -= KEY_RIGHT; }
			break;
		case 'i':
		case 'I':
			//I
			if(KeyFlags & KEY_UP) { KeyFlags -= KEY_UP; }
			break;
		}
	}
}

void mousepressed_handler(mousebutton_t keynum) {
	if(keynum == MB_LEFT) { //Left
		switch_character_move();
	} else if(keynum == MB_RIGHT) { //Right
		use_item();
	} else if(keynum == MB_UP) { //WheelUp
		select_next_item();
	} else if(keynum == MB_DOWN) { //WheelDn
		select_prev_item();
	}
}
