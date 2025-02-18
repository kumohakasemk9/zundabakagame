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

#include "inc/zundagame.h"

#include <string.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>

//gamesys.c
void read_creds();
void local2map(double, double, double*, double*);
void set_speed_for_going_location(int32_t, double, double, double);
void start_command_mode(int32_t);
void switch_character_move();
void execcmd();
void use_item();
void proc_playable_op();
int32_t buy_facility(int32_t fid);
void debug_add_character();
void aim_earth(int32_t);
double get_distance_raw(double, double, double, double);
void select_next_item();
void select_prev_item();
void spawn_playable_me();
void reset_game_cmd();
void ebcount_cmd();
void ebdist_cmd();
void atkgain_cmd();
void cmd_enter();
void cmd_cancel();
void cmd_cursor_back();
void cmd_cursor_forward();
void cmd_putch(char);
void modifyKeyFlags(keyflags_t, int32_t);
void switch_debug_info();

char ChatMessages[MAX_CHAT_COUNT][BUFFER_SIZE]; //Chatmessage storage
int32_t ChatTimeout = 0; //Douncounting timer, chat will shown it this is not 0
GameObjs_t Gobjs[MAX_OBJECT_COUNT]; //Game object memory
int32_t CameraX = 0, CameraY = 0; //Camera position storage
int32_t CursorX, CursorY; //Cursor position storage
obj_type_t AddingTID; //Debug mode adding tid
int32_t CommandCursor = -1; //Command System Related
char CommandBuffer[BUFFER_SIZE]; //Command string buffer
int32_t DebugMode = 0; //If it is 1, you can see hitbox, object ids and more, it can be changed through gdb.
gamestate_t GameState; //Current game status
int32_t StateChangeTimer = 0; //Not it is almost like respawn timer
int32_t PlayingCharacterID = -1; //Playable character's ID for Gobjs[]
int32_t Money; //Money value, increaes when enemy killed, decreases when item purchased
int32_t EarthID; //Earth object ID
int32_t CharacterMove; //If it is 1, playable character will follow mouse
int32_t SelectingItemID; //Changed by mousewheel, holds which item is selected now
extern const int32_t ITEMPRICES[ITEM_COUNT]; //Item price LUT
extern const int32_t FTIDS[ITEM_COUNT]; //Item <-> TID LUT
langid_t LangID; //Language ID for internationalization
int32_t ItemCooldownTimers[ITEM_COUNT]; //Set to nonzero value when item purchased, countdowns, if it got 0 you can buy item again.
extern int32_t ITEMCOOLDOWNS[ITEM_COUNT]; //Default ITEMcooldown LUT
int32_t SkillCooldownTimers[SKILL_COUNT]; //Set to nonzero balue when skill used, countdowns and if it gets 0 you can use skill again
int32_t CurrentPlayableCharacterID = 0; //Current selected playable character id (Not Gobjs id!) for more playables
keyflags_t KeyFlags;
int32_t ProgramExiting = 0; //Program should exit when 1
int32_t MapTechnologyLevel; //Increases when google item placed, buffs playable
int32_t MapEnergyLevel; //Increases when generator item placed, if it is lower than MapRequiredEnergyLevel, then all facilities stop.
int32_t MapRequiredEnergyLevel = 0; //Increases when item placed except generator item.
int32_t SkillKeyState;
extern smpstatus_t SMPStatus;
extern int32_t SMPProfCount;
extern int32_t SelectedSMPProf;
extern SMPProfile_t *SMPProfs;
extern SMPPlayers_t SMPPlayerInfo[MAX_CLIENTS]; //Server's player informations
extern int32_t SMPcid; //ID of our client (told from SMP server)
int32_t StatusShowTimer; //For showstatus(), it countdowns and if it gots 0 status message disappears
char StatusTextBuffer[BUFFER_SIZE]; //For showstatus(), status text storage
int32_t CommandBufferMutex = 0; //If 1, we should not modify CommandBuffer
int32_t DifEnemyBaseCount[4] = {1, 0, 0, 0}; //Default topright, bottomright, topleft and bottomleft enemy boss count
int32_t DifEnemyBaseDist = 500; //Default enemy boss distance from each other
double DifATKGain = 1.00; //Attack damage gain
double GameTickTime; //Game tick running time (avg)
int32_t DebugStatType = 0; //Shows information if nonzero 0: No debug, 1: System profiler, 2: Input test
extern int32_t NetworkTimeout; //If there's period that has no packet longer than this value, assumed as disconnected. 0: disable timeout

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
	if(lookup(tid, &t) == -1) {
		return 0;
	}
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
			if(lookup(Gobjs[parid].tid, &t2) == -1) {
				return -1;
			}
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
			warn("add_character(): object destroyed, no target found.\n");
		}
	}
	if(tid == TID_EARTH) {
		Gobjs[newid].hp = 1; //For initial scene
		
	} else if(tid == TID_ENEMYBASE) {
		//tid = 1, zundamon star
		//find earth and attempt to approach it
		aim_earth(newid);
		Gobjs[newid].hp = 1; //For initial scene
		
	} else if(tid == TID_KUMO9_X24_PCANNON) {
		//Kumo9's pcanon aims nearest facility
		Gobjs[newid].aiming_target = find_nearest_unit(newid, KUMO9_X24_PCANNON_RANGE, UNITTYPE_FACILITY);
		
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
	info("[chat] %s\n", c);
}

void chatf(const char* p, ...) {
	va_list v;
	ssize_t r;
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
	if(lookup((uint8_t)Gobjs[srcid].tid, &t) == -1) {
		return;
	}
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
			if(lookup(Gobjs[i].tid, &dstinfo) == -1 || lookup(Gobjs[srcid].tid, &srcinfo) == -1) {
				return -1;
			}
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
			if(lookup(Gobjs[i].tid, &dstinfo) == -1 || lookup(Gobjs[srcid].tid, &srcinfo) == -1) {
				return -1;
			}
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
	//gametick, called for every 10mS
	#ifndef __WASM
		//prepare for measure running time
		double tbefore = get_current_time_ms();
	#endif

	//Take care of chat timeout and timers
	if(ChatTimeout != 0) { ChatTimeout--; }
	if(StatusShowTimer != 0) { StatusShowTimer--; }

	//SMP Processing
	#ifndef __WASM
		network_recv_task();
	#endif
	if(SMPStatus == NETWORK_LOGGEDIN) {
		process_smp();
	}
	
	//If game is not in playing state, do special process
	if(GameState == GAMESTATE_INITROUND) {
		//Starting animation: Slowly increase enemy base and the earth hp
		for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
			if(Gobjs[i].tid == TID_EARTH || Gobjs[i].tid == TID_ENEMYBASE) {
				LookupResult_t t;
				if(lookup((uint8_t)Gobjs[i].tid, &t) == -1) {
					return;
				}
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
	} else if( GameState == GAMESTATE_GAMECLEAR || GameState == GAMESTATE_GAMEOVER) {
		//Init game in 5 sec.
		StateChangeTimer++;
		//Pause playable character
		CharacterMove = 0;
		if(StateChangeTimer > 500) {
			reset_game();
			return; //Not doing AI proc after init game, or bug.
		}
	} else if( GameState == GAMESTATE_DEAD) {
		//Respawn in 10 sec
		StateChangeTimer--;
		if(StateChangeTimer == 0) {
			spawn_playable_me();
			GameState = GAMESTATE_PLAYING;
		}
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

	procai(); //process all game objects and hitdetects

	#ifndef __WASM
		//Measure running time
		static double avgt = 0;
		static int32_t avgc = 0;
		avgt += get_current_time_ms() - tbefore;
		avgc++;
		//Calculate avg of 10 times, then reset
		if(avgc >= 10) {
			GameTickTime = avgt / 10;
			avgc = 0;
			avgt = 0;
		}
	#endif
}

void proc_playable_op() {
	if(!is_range(PlayingCharacterID, 0, MAX_OBJECT_COUNT - 1)) {
		die("proc_playable_op(): Playable character ID is wrong.\n");
		return;
	}
	if(Gobjs[PlayingCharacterID].tid == -1) {
		warn("proc_playable_op(): Player TID is -1??\n");
		return;
	}
	PlayableInfo_t plinf;
	if(lookup_playable(CurrentPlayableCharacterID, &plinf) == -1) {
		return;
	}
	//Move Playable character
	double tx = 0, ty = 0;
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
	if(SMPStatus == NETWORK_LOGGEDIN) {
		stack_packet(EV_PLAYABLE_LOCATION, Gobjs[PlayingCharacterID].x, Gobjs[PlayingCharacterID].y);
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
	if(!is_range(cid, 0, MAX_OBJECT_COUNT - 1) || !is_range(sid, 0, SKILL_COUNT - 1) ) {
		die("use_skill(): bad parameter!\n");
		return;
	}
	if(!is_playable_character(Gobjs[cid].tid) ) {
		warn("use_skill(): target object is not playable!\n");
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
		if(lookup(Gobjs[i].tid, &t) == -1) {
			return -1;
		}
		if(t.objecttype == UNITTYPE_FACILITY) {
			if(get_distance_raw(Gobjs[i].x, Gobjs[i].y, mx, my) < 500) {
				//showerrorstr(0);
				showstatus( (char*)getlocalizedstring(0));
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


//Command Handler: called when command entered
void execcmd() {
	//Return if command is empty string
	if(strlen(CommandBuffer) == 0) {
		return;
	}

	if(strcmp(CommandBuffer, "/credit") == 0) {
		//Show Credit
		chat(CREDIT_STRING);
	} else if(strcmp(CommandBuffer, "/builddate") == 0) {
		//Show build date
		chat(__TIMESTAMP__);

	} else if(strcmp(CommandBuffer, "/getsmps") == 0) {
		//Get current loaded SMP profile count
		chatf("getsmps: %d", SMPProfCount);

	} else if(memcmp(CommandBuffer, "/getsmp ", 8) == 0) {
		//get selected smp profile info
		get_smp_cmd(&CommandBuffer[8]);

	} else if(strcmp(CommandBuffer, "/getcurrentsmp") == 0) {
		//get current SMP profile.
		getcurrentsmp_cmd();

	} else if(memcmp(CommandBuffer, "/addsmp ", 8) == 0) {
		//Add SMP profile
		add_smp_cmd(&CommandBuffer[8]);

	} else if(strcmp(CommandBuffer, "/reset") == 0) {
		//reset game
		reset_game_cmd();

	} else if(strcmp(CommandBuffer, "/jp") == 0) {
		//Change language to Japanese
		LangID = LANGID_JP;

	} else if(strcmp(CommandBuffer, "/en") == 0) {
		//Change language to English
		LangID = LANGID_EN;

	} else if(memcmp(CommandBuffer, "/chfont ", 8) == 0) {
		//Load font list
		loadfont(&CommandBuffer[8]);

	} else if(memcmp(CommandBuffer, "/connect ", 9) == 0) {
		//Connect to SMP server
		connect_server_cmd(&CommandBuffer[9]);

	} else if(strcmp(CommandBuffer, "/disconnect") == 0) {
		//Disconnect from SMP server
		close_connection_cmd();
	
	} else if(memcmp(CommandBuffer, "/ebcount ", 9) == 0) {
		//Set difficulty parameter: enemy base count
		ebcount_cmd();

	} else if(memcmp(CommandBuffer, "/ebdist ", 8) == 0) {
		//Set difficulty parameter: enemy base distance
		ebdist_cmd();

	} else if(memcmp(CommandBuffer, "/atkgain ", 9) == 0) {
		//Set difficulty parameter: attack gain
		atkgain_cmd();
	
	} else if(strcmp(CommandBuffer, "/difficulty") == 0) {
		//Difficulty query command
		chatf("difficulty: ATKGain: %.2f EBDist: %d EBCount: %d %d %d\n", DifATKGain, DifEnemyBaseDist, DifEnemyBaseCount[0], DifEnemyBaseCount[1], DifEnemyBaseCount[2]);
	
	} else if(memcmp(CommandBuffer, "/chtimeout ", 11) == 0) {
		//Setting timeout command
		changetimeout_cmd(&CommandBuffer[11]);

	} else if(strcmp(CommandBuffer, "/timeout") == 0) {
		//Get timeout
		chatf("timeout: %d", NetworkTimeout);

	} else if(strcmp(CommandBuffer, "/getclients") == 0) {
		//Get connected users
		getclients_cmd();

	} else {
		if(CommandBuffer[0] == '/') {
			chatf( (char*)getlocalizedstring(TEXT_BAD_COMMAND_PARAM) );
		} else {
			if(SMPStatus == NETWORK_LOGGEDIN) {
				stack_packet(EV_CHAT, CommandBuffer);
			} else {
				chatf("[local] %s", CommandBuffer);
			}
		}
	}
}

void ebcount_cmd() {
	//DifEnemyBaseCount set command ( topright [bottomright] [topleft] )
	char *p = &CommandBuffer[9];
	int32_t t[4] = {0, 0, 0, 0};
	for(int32_t i = 0; i < 4; i++) {
		//convert and check
		t[i] = (int32_t)strtol(p, NULL, 10);
		//First param should be greater than or equal 1
		int32_t minim = 0;
		if(i == 0) {
			minim = 1;
		}
		if(!is_range(t[i], minim, 4) ) {
			warn( (char*)getlocalizedstring(TEXT_BAD_COMMAND_PARAM) ); //Bad parameter
			return;
		}

		//find next space and advance pointer, if not found finish converting task.
		char *n = strchr(p, ' ');
		if(n == NULL) { break; }
		p = n + 1;
	}
	
	//Apply
	for(int32_t i = 0; i < 4; i++) {
		DifEnemyBaseCount[i] = t[i];
	}
}

void ebdist_cmd() {
	//DifEnemyBaseDist set command (distance)
	int32_t i = (int32_t)strtol(&CommandBuffer[8], NULL, 10);
	if(!is_range(i, MIN_EBDIST, MAX_EBDIST) ) {
		chat( (char*)getlocalizedstring(TEXT_BAD_COMMAND_PARAM) ); //Bad parameter
		return;
	}
	DifEnemyBaseDist = i;
}

void atkgain_cmd() {
	//DifATKGain set command (atkgain)
	double i = (double)atof(&CommandBuffer[9]);
	if(!is_range_number(i, MIN_ATKGAIN, MAX_ATKGAIN) ) {
		chat( (char*)getlocalizedstring(TEXT_BAD_COMMAND_PARAM) ); //Bad parameter
		return;
	}
	DifATKGain = i;
	info("atkgain_cmd(): atkgain changed to %f\n", DifATKGain);
	if(SMPStatus == NETWORK_LOGGEDIN) {
		stack_packet(EV_CHANGE_ATKGAIN);
	}
}

void reset_game_cmd() {
	//Reset game round, if in SMP, send reset packet instead.
	if(SMPStatus != NETWORK_LOGGEDIN) {
		reset_game();
	} else {
		stack_packet(EV_RESET);
	}
}

//Restarts game
void reset_game() {
	//Initialize game related variables
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
	//Initialize GameObj Slots (Clears map)
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		Gobjs[i].tid = TID_NULL;
	}

	//Add Earth (Ally base, if you lose it game is over.)
	const double START_POS = 200;
	EarthID = add_character(TID_EARTH, START_POS, START_POS, OBJID_INVALID);
	
	//Place enemy zundamon base according to DifEnemyBaseCount (defines how many enemybase spawns by each edges) and DifEnemyBaseDist(defines how far between enemy bases)
	//Top right, bottom right, top left, bottomleft
	const double BASEPOS_X[] = {MAP_WIDTH - START_POS, MAP_WIDTH - START_POS, START_POS, START_POS}; //Exact spawn coordinate for each enemy spawn points
	const double BASEPOS_Y[] = {MAP_HEIGHT - START_POS, START_POS, MAP_HEIGHT - START_POS, START_POS};
	const double MPY_X[] = {-1, -1, 1, 1}; //Which position should we add enemybase for each enemy spawn points
	const double MPY_Y[] = {-1, 1, -1, 1};
	for(int32_t i = 0; i < 4; i++) {
		for(int32_t j = 0; j < DifEnemyBaseCount[i]; j++) {
			//basespawnlocation + (adddirection * enemydistance * count ), will modified to be 2 rows
			double x = BASEPOS_X[i] + (MPY_X[i] * DifEnemyBaseDist * (j % 2) );
			double y = BASEPOS_Y[i] + (MPY_Y[i] * DifEnemyBaseDist * floor(j / 2) );
			add_character(TID_ENEMYBASE, x, y, OBJID_INVALID);
		}
	}

	//Spawn Playables
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
			warn("spawn_playable(): Could not register my playable character id to list.\n");
		}
	}
}

//Spawn specificated playable character by pid at fixed coordinate
int32_t spawn_playable(int32_t pid) {
	PlayableInfo_t t;
	if(lookup_playable(pid, &t) == -1) {
		return 0;
	}
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

int32_t gameinit(char* fn) {
	//Gameinit function, load assets and more. Called when program starts.
	info("Welcome to zundagame.\n");
	info("%s\n", CONSOLE_CREDIT_STRING);
	info("Build date: %s\n", __TIMESTAMP__);
	
	//Debug
	info("sizeof(event_hdr_t): %ld\n", sizeof(event_hdr_t) );
	info("sizeof(np_greeter_t): %ld\n", sizeof(np_greeter_t) );
	info("sizeof(userlist_hdr_t): %ld\n", sizeof(userlist_hdr_t) );
	info("sizeof(ev_placeitem_t): %ld\n", sizeof(ev_placeitem_t) );
	info("sizeof(ev_useskill_t): %ld\n", sizeof(ev_useskill_t) );
	info("sizeof(ev_playablelocation_t): %ld\n", sizeof(ev_playablelocation_t) );
	info("sizeof(ev_chat_t): %ld\n", sizeof(ev_chat_t) );
	info("sizeof(ev_reset_t): %ld\n", sizeof(ev_reset_t) );
	info("sizeof(ev_hello_t): %ld\n", sizeof(ev_hello_t) );
	info("sizeof(ev_bye_t): %ld\n", sizeof(ev_bye_t) );
	info("sizeof(ev_changeatkgain_t): %ld\n", sizeof(ev_changeatkgain_t) );
	
	#ifndef __WASM
		read_creds(fn); //read smp profiles
		if(init_graphics() == -1) { //Graphics init
			fail("init_graphics() failed.\n");
			return -1;
		}
		detect_syslang();
	#else
		info("Compiled with -D__WASM\n, skipping graphics, file io and language autodetect feature.\n");
	#endif
	
	//Initialize Chat Message Slots
	for(uint8_t i = 0; i < MAX_CHAT_COUNT; i++) {
		ChatMessages[i][0] = 0;
	}
	
	check_data(); //Data Check
	
	reset_game(); //reset game round
	return 0;
}

void do_finalize() {
	info("do_finalize(): bye. thank you for playing.\n");
	//Finalize
	ProgramExiting = 1; //Notify that program is exiting
	
	#ifndef __WASM
		//Clean SMP profiles
		if(SMPProfs != NULL) {
			free(SMPProfs);
		}
		
		//Close Network Connection
		if(SMPStatus != NETWORK_DISCONNECTED) {
			close_tcp_socket();
			SMPStatus = NETWORK_DISCONNECTED;
		}
		uninit_graphics();
	#endif
}

//Read SMP Credentials from file
void read_creds(char *fn) {
	FILE* f = fopen(fn, "r");
	char buf[BUFFER_SIZE];
	if(f == NULL) {
		warn("Can not open %s: %s\n", fn, strerror(errno) );
		return;
	}
	int lineno = 0;
	while(fgets(buf, BUFFER_SIZE, f) != NULL) {
		buf[BUFFER_SIZE - 1] = 0; //For Additional security
		lineno++;
		if(add_smp_profile(buf, '\t') == -1) {
			warn("at %s line %d\n", fn, lineno);
		}
	}
	fclose(f);
}

//Execute typed command and exit command mode
void cmd_enter() {
	CommandCursor = -1;
	execcmd();
}

//Cancel command mode without execute
void cmd_cancel() {
	CommandCursor = -1;
}

//Move cursor back in command mode
void cmd_cursor_back() {
	if(CommandCursor > 0) {CommandCursor--;}
}

//Move cursor forward in command mode
void cmd_cursor_forward() {
	if(CommandCursor < utf8_strlen(CommandBuffer) ) {
		CommandCursor++;
	}
}

//Remove a single letter before cursor in command mode
void cmd_backspace() {
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
}

//Put single letter after cursor in command mode
void cmd_putch(char c) {
	if(is_range(c, 0x20, 0x7e) && !CommandBufferMutex) {
		CommandBufferMutex = 1;
		char s[] = {c, 0};
		if(strlen(CommandBuffer) + strlen(s) + 1 <= sizeof(CommandBuffer) ) {
			utf8_insertstring(CommandBuffer, s, CommandCursor, sizeof(CommandBuffer) );
			CommandCursor += 1;
		} else {
			warn("commandmode_keyhandler(): Append letter failed: Buffer full.\n");
		}
		CommandBufferMutex = 0;
	}
}

//Set or reset bit of KeyFlags, s 1: set, 0: reset; f: bitmask
void modifyKeyFlags(keyflags_t f, int32_t s) {
	if(s == 0) {
		KeyFlags &= ~f;
	} else {
		KeyFlags |= f;
	}
}

void mousemotion_handler(int32_t x, int32_t y) {
	//Called when mouse moved, save mouse location value
	CursorX = (int32_t)x;
	CursorY = (int32_t)y;
}

//Switch debug info
void switch_debug_info() {
	DebugStatType++;
	if(DebugStatType > 3) {DebugStatType = 0;} //0 to 2	
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
		default:
			if(ks == SPK_F3) {
				//F3 Key
				switch_debug_info();
			}
		}
	} else {
		//command mode
		//printf("%02d\n", kc);
		if(ks == SPK_ENTER) {
			//Enter
			cmd_enter();
		} else if(ks == SPK_BS) {
			//BackSpace
			cmd_backspace();
		} else if(ks == SPK_LEFT) {
			//LeftArrow
			cmd_cursor_back();
		} else if(ks == SPK_RIGHT) {
			//RightArrow
			cmd_cursor_forward();
		} else if(kc == 0x16) {
			//CtrlV
			//gdk_clipboard_read_text_async(GClipBoard, NULL, clipboard_read_handler, NULL);
		} else if(ks == SPK_ESC) {
			//Escape
			cmd_cancel();
		} else {
			//Others
			//printf("%d\n", keycode);
			cmd_putch(kc);
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
