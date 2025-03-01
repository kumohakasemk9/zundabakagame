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

//Game settings
#define CHAT_TIMEOUT 1000 //Chat message timeout
#define ERROR_SHOW_TIMEOUT 500 //Error message timeout

#define CREDIT_STRING "Zundamon bakage (C) 2024 Kumohakase https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0, Zundamon is from https://zunko.jp/ (C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr"
#define CONSOLE_CREDIT_STRING "Zundamon bakage (C) 2024 Kumohakase https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0\n" \
							  "Zundamon is from https://zunko.jp/ (C) 2024 ＳＳＳ合同会社\n" \
							  "(C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr\n" \
							  "Please consider supporting me through ko-fi or pateron\n" \
							  "https://ko-fi.com/kumohakase\n" \
							  "https://www.patreon.com/kumohakasemk8\n"

#include "inc/zundagame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>

//gamesys.c
void read_creds();
void local2map(double, double, double*, double*);
void start_command_mode(int32_t);
void switch_character_move();
void execcmd();
void use_item();
void proc_playable_op();
int32_t buy_facility(int32_t fid);
void debug_add_character();
void select_next_item();
void select_prev_item();
void reset_game_cmd();
int32_t ebcount_cmd(char*);
int32_t ebdist_cmd(char*);
int32_t atkgain_cmd(char*);
int32_t chspawn_cmd(char*);
void cmd_enter();
void cmd_cancel();
void cmd_cursor_back();
void cmd_cursor_forward();
void cmd_putch(char);
void switch_debug_info();
int32_t changeplayable_cmd(char*);
void read_blocked_users();
void commit_menu();
void go_title();
void select_next_menuitem();
void select_prev_menuitem();
void title_item_change(int32_t);

int32_t SelectingMenuItem = 0;
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
int32_t PlayingCharacterID = -1; //Playable character's object ID in Gobjs[]
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
int32_t CurrentPlayableID; //Current selected playable character id (Not Gobjs id!) for more playables
int32_t PlayableID = 0; //Current selected playable character id (Do not confuse it with PlayingCharacterID)
keyflags_t KeyFlags; //Key Bitmap
int32_t ProgramExiting = 0; //Program should exit when 1
int32_t MapTechnologyLevel; //Increases when google item placed, buffs playable
int32_t MapEnergyLevel; //Increases when generator item placed, if it is lower than MapRequiredEnergyLevel, then all facilities stop.
int32_t MapRequiredEnergyLevel = 0; //Increases when item placed except generator item.
int32_t SkillKeyState; //Pushing skill key number
extern smpstatus_t SMPStatus;
extern int32_t SMPProfCount;
extern int32_t SelectedSMPProf;
extern SMPProfile_t *SMPProfs;
extern SMPPlayers_t SMPPlayerInfo[MAX_CLIENTS]; //Server's player informations
extern int32_t SMPcid; //ID of our client (told from SMP server)
int32_t StatusShowTimer; //it countdowns and if it gots 0 status message disappears
int32_t StatusTextID; //status text id, shown in command bar
int32_t CommandBufferMutex = 0; //If 1, we should not modify CommandBuffer
int32_t DifEnemyBaseCount[4] = {1, 0, 0, 0}; //Default topright, bottomright, topleft and bottomleft enemy boss count
int32_t DifEnemyBaseDist = 500; //Default enemy boss distance from each other
double DifATKGain = 1.00; //Attack damage gain
double GameTickTime; //Game tick running time (avg)
int32_t DebugStatType = 0; //Shows information if nonzero 0: No debug, 1: System profiler, 2: Input test
extern int32_t NetworkTimeout; //If there's period that has no packet longer than this value, assumed as disconnected. 0: disable timeout
int32_t SpawnRemain; //playable character can not respawn if it is 0.
int32_t InitSpawnRemain = -1; //how many times can playable character respawn?
extern int32_t BlockedUsersCount;
extern char** BlockedUsers;
int32_t LocatorType = 0; //Game background type
extern int32_t TitleSkillTimer;
int32_t SelectingHelpPage;
int32_t EndlessMode = 0; //Endless mode; no damaging earth and enemy base, unlimited skills, no respawn delay

//Translate window coordinate into map coordinate
void local2map(double localx, double localy, double* mapx, double* mapy) {
	*mapx = CameraX + localx;
	*mapy = CameraY + (WINDOW_HEIGHT - localy);
}

//Translate map coordinate to window coordinate
void map2local(double mapx, double mapy, double *localx, double *localy) {
	*localx = mapx - CameraX;
	*localy = WINDOW_HEIGHT - (mapy - CameraY);
}

//Get local (window) coordinate of character
void getlocalcoord(int32_t i, double *x, double *y) {
	if(!is_range(i, 0, MAX_OBJECT_COUNT - 1)){
		die("getlocalcoord(): bad parameter passed!\n");
		return;
	}
	*x = Gobjs[i].x - CameraX;
	*y = WINDOW_HEIGHT - (Gobjs[i].y - CameraY);
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
		warn("gamesys.c: chatf() failed. Buffer overflow or vsnprintf() failed.\n");
		return;
	}
	chat(b);
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
	network_recv_task();
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
	
	//Move Playable character
	double tx = 0, ty = 0;
	if(CharacterMove && GameState == GAMESTATE_PLAYING) {
		//If CharacterMove == 1, and PLAYING state, playable character will follow mouse
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
	if(SMPStatus == NETWORK_LOGGEDIN ) {
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
					PlayableInfo_t plinf;
					if(lookup_playable(CurrentPlayableID, &plinf) == -1) {
						return;
					}
					//Dedicated key pressed before
					SkillKeyState = -1;
					use_skill(PlayingCharacterID, i, plinf);
					if(SMPStatus == NETWORK_LOGGEDIN) {
						stack_packet(EV_USE_SKILL, i); //if logged in to SMP server, notify event instead
					}
					//Reset Skill CD
					if(DebugMode || EndlessMode) {
						SkillCooldownTimers[i] = plinf.skillinittimers[i];
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
				set_cmdstatus(0); //Too close!
				return 1;
			}
		}
	}
	//If connected to remote server, notify item place.
	if(SMPStatus == NETWORK_LOGGEDIN) {
		stack_packet(EV_PLACE_ITEM, tid, mx, my);
	} else {
		add_character(tid, mx, my, PlayingCharacterID);
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

//Select prev menuitem
void select_prev_menuitem() {
	if(GameState != GAMESTATE_TITLE) {
		return;
	}
	if(SelectingMenuItem < MAX_MENU_STRINGS - 1) {
		SelectingMenuItem++;
	} else {
		SelectingMenuItem = 0;
	}
}

//Select_next menuitem
void select_next_menuitem() {
	if(GameState != GAMESTATE_TITLE) {
		return;
	}
	if(SelectingMenuItem > 0) {
		SelectingMenuItem--;
	} else {
		SelectingMenuItem = MAX_MENU_STRINGS - 1;
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
		strcpy(CommandBuffer, ":");
		CommandCursor = 1;
	} else {
		CommandBuffer[0] = 0;
		CommandCursor = 0;
	}
}

int32_t insert_cmdbuf(char* data) {
	int32_t f = 1;
	if(CommandBufferMutex == 1 || CommandCursor == -1) {
		warn("insert_cmdbuf(): not available.\n");
		return -1;
	}
	CommandBufferMutex = 1;
	if(utf8_insertstring(CommandBuffer, data, CommandCursor, sizeof(CommandBuffer) ) == 0) {
		CommandCursor += utf8_strlen(data);
	} else {
		warn("insert_cmdbuf(): insert failed.\n");
		f = -1;
	}
	CommandBufferMutex = 0;
	return f;
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
	
	int32_t errid = -1;
	if(strcmp(CommandBuffer, ":credit") == 0) {
		//Show Credit
		chat(CREDIT_STRING);
	} else if(strcmp(CommandBuffer, ":builddate") == 0) {
		//Show build date
		chat(__TIMESTAMP__);

	} else if(strcmp(CommandBuffer, ":getsmps") == 0) {
		//Get current loaded SMP profile count
		chatf("getsmps: %d", SMPProfCount);

	} else if(memcmp(CommandBuffer, ":getsmp ", 8) == 0) {
		//get selected smp profile info
		errid = get_smp_cmd(&CommandBuffer[8]);

	} else if(strcmp(CommandBuffer, ":getcurrentsmp") == 0) {
		//get current SMP profile.
		errid = getcurrentsmp_cmd();

	} else if(memcmp(CommandBuffer, ":addsmp ", 8) == 0) {
		//Add SMP profile
		errid = add_smp_cmd(&CommandBuffer[8]);

	} else if(strcmp(CommandBuffer, ":reset") == 0) {
		//reset game
		reset_game_cmd();

	} else if(strcmp(CommandBuffer, ":jp") == 0) {
		//Change language to Japanese
		LangID = LANGID_JP;

	} else if(strcmp(CommandBuffer, ":en") == 0) {
		//Change language to English
		LangID = LANGID_EN;

	} else if(memcmp(CommandBuffer, ":chfont ", 8) == 0) {
		//Load font list
		loadfont(&CommandBuffer[8]);

	} else if(memcmp(CommandBuffer, ":connect ", 9) == 0) {
		//Connect to SMP server
		errid = connect_server_cmd(&CommandBuffer[9]);

	} else if(strcmp(CommandBuffer, ":disconnect") == 0) {
		//Disconnect from SMP server
		close_connection_cmd();
	
	} else if(memcmp(CommandBuffer, ":ebcount ", 9) == 0) {
		//Set difficulty parameter: enemy base count
		errid = ebcount_cmd(&CommandBuffer[9]);

	} else if(memcmp(CommandBuffer, ":ebdist ", 8) == 0) {
		//Set difficulty parameter: enemy base distance
		errid = ebdist_cmd(&CommandBuffer[8]);

	} else if(memcmp(CommandBuffer, ":atkgain ", 9) == 0) {
		//Set difficulty parameter: attack gain
		errid = atkgain_cmd(&CommandBuffer[9]);
	
	} else if(strcmp(CommandBuffer, ":difficulty") == 0) {
		//Difficulty query command
		chatf("difficulty: ATKGain: %.2f EBDist: %d EBCount: %d %d %d Endless: %d\n", DifATKGain, DifEnemyBaseDist, DifEnemyBaseCount[0], DifEnemyBaseCount[1], DifEnemyBaseCount[2], (char*)get_localized_bool(EndlessMode) );

	} else if(memcmp(CommandBuffer, ":chspawn ", 9) == 0) {
		//Allowable spawn count change cmd
		errid = chspawn_cmd(&CommandBuffer[9]);

	} else if(memcmp(CommandBuffer, ":chtimeout ", 11) == 0) {
		//Setting timeout command
		errid = changetimeout_cmd(&CommandBuffer[11]);

	} else if(strcmp(CommandBuffer, ":timeout") == 0) {
		//Get timeout
		chatf("timeout: %d", NetworkTimeout);

	} else if(strcmp(CommandBuffer, ":getclients") == 0) {
		//Get connected users
		errid = getclients_cmd();

	} else if(memcmp(CommandBuffer, ":chplayable ", 12) == 0 ) {
		//Change playable
		errid = changeplayable_cmd(&CommandBuffer[12]);

	} else if(strcmp(CommandBuffer, ":getplayable") == 0) {
		//Get playable
		chatf("getplayable: %d", PlayableID);

	} else if(memcmp(CommandBuffer, ":ignore ", 8) == 0) {
		//Add blocked user
		errid = addusermute_cmd(&CommandBuffer[8]);

	} else if(memcmp(CommandBuffer, ":listen ", 8) == 0) {
		//Delete blocked user
		errid = delusermute_cmd(&CommandBuffer[8]);
		
	} else if(strcmp(CommandBuffer, ":listmuted") == 0) {
		//list blocked user
		errid = listusermute_cmd();

	} else if(strcmp(CommandBuffer, ":togglechat") == 0) {
		//Togglechat cmd
		errid = togglechat_cmd();

	} else if(strcmp(CommandBuffer, ":title") == 0) {
		//title command
		go_title();
	
	} else if(strcmp(CommandBuffer, ":endless") == 0) {
		EndlessMode = 1;

	} else if(strcmp(CommandBuffer, ":noendless") == 0) {
		EndlessMode = 0;

	} else {
		if(CommandBuffer[0] == ':') {
			set_cmdstatus(TEXT_BAD_COMMAND_PARAM);
		} else {
			if(SMPStatus == NETWORK_LOGGEDIN) {
				stack_packet(EV_CHAT, CommandBuffer);
			} else {
				chatf("[local] %s", CommandBuffer);
			}
		}
	}

	set_cmdstatus(errid);
}

void set_cmdstatus(int32_t sid) {
	StatusShowTimer = ERROR_SHOW_TIMEOUT;
	StatusTextID = sid;
}

void go_title() {
	if(SMPStatus != NETWORK_DISCONNECTED) {
		close_connection_cmd();
	}
	TitleSkillTimer = 0;
	SelectingMenuItem = 0;
	SelectingHelpPage = -1;
	GameState = GAMESTATE_TITLE;
	reset_game();
}

int32_t changeplayable_cmd(char *param) {
	//Change playable character for next round
	int32_t i = atoi(param);
	if(!is_range(i, 0, PLAYABLE_CHARACTERS_COUNT - 1) ) {
		warn("changeplayable_cmd(): no such playable character\n");
		return TEXT_BAD_COMMAND_PARAM; // Bad param
	}
	if(SMPStatus == NETWORK_LOGGEDIN) {
		stack_packet(EV_CHANGE_PLAYABLE_ID);
	} else {
		info("Change Playableid to %d\n", PlayableID);
		PlayableID = i;
	}
	return -1;
}

int32_t chspawn_cmd(char* param) {
	//Change allowed spawn count
	int32_t i = atoi(param);
	if(is_range(i, -1, MAX_SPAWN_COUNT) ) {
		InitSpawnRemain = i;
	} else {
		warn("chspawn_cmd(): parameter range check fail\n");
		return TEXT_BAD_COMMAND_PARAM; //Bad parameter
	}
	return -1;
}

int32_t ebcount_cmd(char *p) {
	//DifEnemyBaseCount set command ( topright [bottomright] [topleft] )
	int32_t t[4] = {0, 0, 0, 0};
	for(int32_t i = 0; i < 4; i++) {
		//convert and check
		t[i] = (int32_t)strtol(p, NULL, 10);
		//First param should be greater than or equal 1
		int32_t minim = 0;
		if(i == 0) {
			minim = 1;
		}
		if(!is_range(t[i], minim, MAX_EBCOUNT) ) {
			warn("ebcount_cmd(): parameter range check failed.\n");
			return TEXT_BAD_COMMAND_PARAM; //Bad parameter
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
	return -1;
}

int32_t ebdist_cmd(char *p) {
	//DifEnemyBaseDist set command (distance)
	int32_t i = (int32_t)strtol(p, NULL, 10);
	if(!is_range(i, MIN_EBDIST, MAX_EBDIST) ) {
		warn("ebdist_cmd(): parameter range check fail\n");
		return TEXT_BAD_COMMAND_PARAM; //bad param
	}
	DifEnemyBaseDist = i;
	return -1;
}

int32_t atkgain_cmd(char *p) {
	//DifATKGain set command (atkgain)
	double i = (double)atof(p);
	if(!is_range_number(i, MIN_ATKGAIN, MAX_ATKGAIN) ) {
		warn("atkgain_cmd(): parameter range check fail.\n");
		return TEXT_BAD_COMMAND_PARAM; //Bad parameter
	}
	if(SMPStatus == NETWORK_LOGGEDIN) {
		stack_packet(EV_CHANGE_ATKGAIN);
	} else {
		DifATKGain = i;
		info("atkgain_cmd(): atkgain changed to %f\n", DifATKGain);
	}
	return -1;
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
	MapTechnologyLevel = 0;
	MapEnergyLevel = 0;
	Money = 0;
	SelectingItemID = -1;
	CameraX = 0;
	CameraY = 0;
	SkillKeyState = -1;
	SpawnRemain = InitSpawnRemain;
	EarthID = -1;
	PlayingCharacterID = -1;
	CurrentPlayableID = PlayableID;
	//Init Skill state and timer
	for(uint8_t i = 0; i < SKILL_COUNT; i++) {
		SkillCooldownTimers[i] = 0;
	}
	//Initialize Cooldown Timer
	for(uint8_t i = 0; i < ITEM_COUNT; i++) {
		ItemCooldownTimers[i] = 0;
	}
	//Init Gameobjs
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		Gobjs[i].tid = TID_NULL;
	}

	//In TITLE state, skip state changing and preparing enemy, playable
	if(GameState == GAMESTATE_TITLE) {
		//Add demo character instead
		for(int32_t i = 0; i < 3; i++) {
			add_character(TID_ENEMYBASE, WINDOW_WIDTH + ( (i + 1) * 100), WINDOW_HEIGHT, OBJID_INVALID);
		}
		add_character(TID_EARTH, 150, 150, OBJID_INVALID);
		add_character(TID_FORT, 500, 150, OBJID_INVALID);
		return;
	}
	GameState = GAMESTATE_INITROUND;
	
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
				respawn_smp_player(i);
			}
		}
	}
}

//Spawn playable character
void spawn_playable_me() {
	CharacterMove = 0;
	PlayingCharacterID = spawn_playable(CurrentPlayableID);
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

int32_t gameinit(char* fn) {
	//Gameinit function, load assets and more. Called when program starts.
	info("Welcome to zundagame.\n");
	info("%s\n", CONSOLE_CREDIT_STRING);
	info("Build date: %s\n", __TIMESTAMP__);
	
	//Debug
	info("sizeof(event_hdr_t): %ld\n", sizeof(event_hdr_t) );
	info("sizeof(np_greeter_t): %ld\n", sizeof(np_greeter_t) );
	info("sizeof(ev_placeitem_t): %ld\n", sizeof(ev_placeitem_t) );
	info("sizeof(ev_useskill_t): %ld\n", sizeof(ev_useskill_t) );
	info("sizeof(ev_chat_t): %ld\n", sizeof(ev_chat_t) );
	info("sizeof(ev_reset_t): %ld\n", sizeof(ev_reset_t) );
	info("sizeof(ev_changeatkgain_t): %ld\n", sizeof(ev_changeatkgain_t) );
	
	#ifndef __WASM
		read_creds(fn); //read smp profiles
		read_blocked_users(); //read smp blocked users
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
	go_title();

	return 0;
}

void do_finalize() {
	info("do_finalize(): bye. thank you for playing.\n");
	//Finalize
	ProgramExiting = 1; //Notify that program is exiting
	
	//Clean SMP profiles
	if(SMPProfs != NULL) {
		free(SMPProfs);
	}

	//Clean blocked user list
	if(BlockedUsers != NULL) {
		#ifndef __WASM
			//save blocked user list in nonwasm edition
			FILE *fwbl = fopen("blocked_users", "w");
			if(fwbl == NULL) {
				warn("Saving block user list failed: %s\n", strerror(errno) );
			}
		#endif

		for(int32_t i = 0; i < BlockedUsersCount; i++) {
			char *e = BlockedUsers[i];
			if(e != NULL) {
				#ifndef __WASM
					if(fwbl != NULL) {
						fprintf(fwbl, "%s\n", e);
					}
				#endif
				free(e);
			}
		}
		free(BlockedUsers);
		#ifndef __WASM
			if(fwbl != NULL) {
				fclose(fwbl);
			}
		#endif
	}
	#ifndef __WASM
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

//Read blocked user list from file
void read_blocked_users() {
	FILE *f = fopen("blocked_users", "r");
	char buf[BUFFER_SIZE];
	if(f == NULL) {
		warn("Can not open blocked_users: %s\n", strerror(errno) );
		return;
	}
	while(fgets(buf, BUFFER_SIZE, f) != NULL) {
		buf[BUFFER_SIZE - 1] = 0; //For Additional security
		for(int32_t i = 0; i < strlen(buf); i++) {
			if(buf[i] == '\r' || buf[i] == '\n') {
				buf[i] = 0;
			}
		}
		if(strlen(buf) == 0) {
			continue;
		}
		addusermute(buf);
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

void switch_locator() {
	LocatorType++;
	if(LocatorType > 3) { LocatorType = 0; }
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
		case 'T':
		case 't':
			//T
			start_command_mode(0);
			break;
		case ':':
			//:
			start_command_mode(1);
			break;
		case ' ':
			//space
			if(GameState != GAMESTATE_TITLE) {
				switch_character_move();
			} else {
				commit_menu();
			}
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
		case 'h':
		case 'H':
			//H
			if( (KeyFlags & KEY_HELP) == 0 ) { KeyFlags += KEY_HELP; }
			break;
		case 'u':
		case 'U':
			switch_locator();
			break;
		default:
			if(ks == SPK_F3) {
				//F3 Key
				switch_debug_info();
			}
			if(GameState == GAMESTATE_TITLE) {
				if(ks == SPK_UP) {
					//Up key
					select_next_menuitem();
				} else if(ks == SPK_DOWN) {
					//DownKey
					select_prev_menuitem();
				} else if(ks == SPK_LEFT) {
					//Left key
					title_item_change(-1);
				} else if(ks == SPK_RIGHT) {
					//right key
					title_item_change(1);
				}
			}
		}
	} else {
		//command mode
		//printf("%02d\n", kc);
		if(kc == '\r') {
			//Enter
			cmd_enter();
		} else if(kc == '\b') {
			//BackSpace
			cmd_backspace();
		} else if(ks == SPK_LEFT) {
			//LeftArrow
			cmd_cursor_back();
		} else if(ks == SPK_RIGHT) {
			//RightArrow
			cmd_cursor_forward();
		} else if(kc == 0x1b) {
			//Escape
			cmd_cancel();
		} else if(0x20 <= kc && kc <= 0x7e) {
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
		case 'H':
		case 'h':
			//H
			if(KeyFlags & KEY_HELP) { KeyFlags -= KEY_HELP; }
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
		if(GameState == GAMESTATE_TITLE) {
			title_item_change(1);
		} else {
			select_next_item();
		}
	} else if(keynum == MB_DOWN) { //WheelDn
		if(GameState == GAMESTATE_TITLE) {
			title_item_change(-1);
		} else {
			select_prev_item();
		}
	}
}

//space key handler when game is in TITLE state
void commit_menu() {
	if(GameState != GAMESTATE_TITLE) {
		return;
	}
	if(SelectingHelpPage != -1) {
		SelectingHelpPage++;
		if(SelectingHelpPage > HELP_PAGE_MAX - 2) {
			SelectingHelpPage = -1;
			return;
		}
		return;
	} 
	if(SelectingMenuItem == 0) {
		//Go to game
		GameState = GAMESTATE_INITROUND;
		reset_game();
	} else if(SelectingMenuItem == 1) {
		//Instruction Manual
		SelectingHelpPage = 0;
	} else if(SelectingMenuItem == 8) {
		EndlessMode = ~EndlessMode;
	}
}

void title_item_change(int amount) {
	if(SelectingMenuItem == 2) {
		//Atkgain
		double t = DifATKGain;
		if(amount > 0) {
			t += 0.1;
		} else {
			t -= 0.1;
		}
		DifATKGain = constrain_number(t, MIN_ATKGAIN, MAX_ATKGAIN);
	} else if(SelectingMenuItem == 3) {
		//EnemyBasesDistance
		int32_t t = DifEnemyBaseDist;
		if(amount > 0) {
			t += 50;
		} else {
			t -= 50;
		}
		DifEnemyBaseDist = constrain_i32(t, MIN_EBDIST, MAX_EBDIST);
	} else if(4 <= SelectingMenuItem && SelectingMenuItem <= 6) {
		//EnemyBaseCount 0 - 3
		int32_t t = DifEnemyBaseCount[SelectingMenuItem - 4] + amount;
		int32_t m = 0;
		if(SelectingMenuItem == 4) {
			m = 1;
		}
		DifEnemyBaseCount[SelectingMenuItem - 4] = constrain_i32(t, m, MAX_EBCOUNT);
	} else if(SelectingMenuItem == 7) {
		//Spawn limit
		int32_t t = InitSpawnRemain + amount;
		InitSpawnRemain = constrain_i32(t, -1, MAX_SPAWN_COUNT);
	} else if(SelectingMenuItem == 8) {
		int32_t t = PlayableID + amount;
		PlayableID = constrain_i32(t, 0, PLAYABLE_CHARACTERS_COUNT - 1);
	}
}
