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


extern char ChatMessages[MAX_CHAT_COUNT][BUFFER_SIZE];
extern uint16_t ChatTimeout;
extern GameObjs_t Gobjs[MAX_OBJECT_COUNT];
extern uint16_t CameraX, CameraY;
extern uint16_t CursorX, CursorY;
extern obj_type_t AddingTID;
extern int16_t CommandCursor; //Command System Related
extern char CommandBuffer[BUFFER_SIZE];
gboolean CommandBufferMutex = FALSE;
extern GdkClipboard* GClipBoard;
extern gboolean DebugMode;
extern gamestate_t GameState;
extern uint16_t StateChangeTimer;
extern int16_t PlayingCharacterID;
extern uint32_t Money;
extern uint16_t EarthID;
extern gboolean CharacterMove;
extern int8_t SelectingItemID;
extern const uint16_t ITEMPRICES[ITEM_COUNT];
extern const int8_t FTIDS[ITEM_COUNT];
extern uint16_t ErrorShowTimer;
extern int8_t RecentErrorId;
extern langid_t LangID;
extern double PlayerSpeed;
extern uint16_t ItemCooldownTimers[ITEM_COUNT];
extern uint16_t ITEMCOOLDOWNS[ITEM_COUNT];
extern uint16_t SkillCooldownTimers[SKILL_COUNT];
extern int8_t CurrentPlayableCharacterID;
extern keyflags_t KeyFlags;
extern gboolean ProgramExiting;

//Translate local coordinate into global coordinate
void local2map(double localx, double localy, double* mapx, double* mapy) {
	*mapx = CameraX + localx;
	*mapy = CameraY + (WINDOW_HEIGHT - localy);
}

//Translate map coordinate into local coordinate
//void map2local(double mapx, double mapy, double* localx, double *localy) {
//	*localx = mapx - CameraX;
//	*localy = WINDOW_HEIGHT - (mapy - CameraY);
//}

void getlocalcoord(uint16_t i, double *x, double *y) {
	if(!is_range(i, 0, MAX_OBJECT_COUNT - 1)){
		die("getlocalcoord(): bad parameter passed!\n");
		return;
	}
	*x = Gobjs[i].x - CameraX;
	*y = WINDOW_HEIGHT - (Gobjs[i].y - CameraY);
}

//Add character into game object buffer, returns -1 if fail.
//tid (character id), x, y(initial pos)
uint16_t add_character(uint8_t tid, double x, double y) {
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		if(Gobjs[i].tid != TID_NULL) { continue; } //Skip occupied slot
		LookupResult_t t;
		lookup(tid, &t);
		Gobjs[i].imgid = t.initimgid;
		Gobjs[i].tid = (int8_t)tid;
		Gobjs[i].x = x;
		Gobjs[i].y = y;
		Gobjs[i].hp = (double)t.inithp;
		Gobjs[i].sx = 0;
		Gobjs[i].sy = 0;
		Gobjs[i].aiming_target = OBJID_INVALID;
		Gobjs[i].timer0 = 0;
		Gobjs[i].timer1 = 0;
		Gobjs[i].timer2 = 0;
		Gobjs[i].hitdiameter = t.inithitdiameter;
		Gobjs[i].timeout = t.timeout;
		Gobjs[i].damage = t.damage;
		//MISSLIE, bullet and laser will try to aim closest enemy unit
		if(tid == TID_MISSILE || tid == TID_ALLYBULLET || tid == TID_ENEMYBULLET) {
			//Set initial target
			int16_t r = find_nearest_unit(i, DISTANCE_INFINITY, UNITTYPE_FACILITY | UNITTYPE_BULLET_INTERCEPTABLE | UNITTYPE_UNIT);
			if(r != -1) {
				Gobjs[i].aiming_target = r;
				set_speed_for_following(i);
			} else {
				Gobjs[i].tid = TID_NULL;
				g_print("gamesys.c: add_character(): object destroyed, no target found. id:%d\n", i);
			}
		}
		switch(tid) {
		case TID_EARTH:
			Gobjs[i].hp = 1; //For initial scene
			break;
		case TID_ZUNDAMON3:
			aim_earth(i); //find earth and attempt to approach it
			break;
		case TID_ENEMYBASE:
			//tid = 1, zundamon star
			//find earth and attempt to approach it
			aim_earth(i);
			Gobjs[i].hp = 1; //For initial scene
			break;
		}
		return i;
	}
	die("gamesys.c: add_character(): Gameobj is full!\n");
	return 0;
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
double get_distance(uint16_t id1, uint16_t id2){
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
void set_speed_for_following(uint16_t srcid) {
	if(!is_range(srcid, 0, MAX_OBJECT_COUNT - 1)) {
		die("gamesys.c: set_speed_for_following(): bad srcid passed!\n");
		return;
	}
	int16_t targetid = Gobjs[srcid].aiming_target;
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
void set_speed_for_going_location(uint16_t srcid, double dstx, double dsty, double speedlimit) {
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
int16_t find_nearest_unit(uint16_t srcid, uint16_t finddist, facility_type_t cfilter) {
	double mindist = finddist / 2;
	int16_t t = -1;
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
			lookup((uint8_t)Gobjs[i].tid, &dstinfo);
			lookup((uint8_t)Gobjs[srcid].tid, &srcinfo);
			//if objects are in same team, exclude from searching
			if(dstinfo.teamid != srcinfo.teamid && (cfilter & dstinfo.objecttype) ) {
				double dist = get_distance(i, srcid);
				//find nearest object
				if(dist < mindist) {
					mindist = dist;
					t = (int16_t)i;
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
int16_t find_random_unit(uint16_t srcid, uint16_t finddist, facility_type_t cfilter) {
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
			lookup((uint8_t)Gobjs[i].tid, &dstinfo);
			lookup((uint8_t)Gobjs[srcid].tid, &srcinfo);
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
		return (int16_t)findobjs[selnum];
	}
}

gboolean gametick(gpointer data) {
	//gametick, called for every 10mS
	//Take care of chat timeout and timers
	if(ChatTimeout != 0) { ChatTimeout--; }
	if(ErrorShowTimer != 0) { ErrorShowTimer--; }
	//If game is not in playing state, do special process
	switch(GameState) {
	case GAMESTATE_INITROUND:
		//Slowly increase enemy base and the earth hp
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
		if(StateChangeTimer > 500) {
			reset_game();
			return TRUE; //Not doing AI proc after init game, or bug.
		}
		break;
	case GAMESTATE_DEAD:
		//Respawn in 10 sec
		StateChangeTimer--;
		if(StateChangeTimer == 0) {
			PlayingCharacterID = (int16_t)add_character(TID_KUMO9_X24_ROBOT, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
			CharacterMove = FALSE;
			GameState = GAMESTATE_PLAYING;
		}
		break;
	default:
		break;
	}
	if(GameState == GAMESTATE_PLAYING) {
		//Process Playable Character Operation
		if(is_range(PlayingCharacterID, 0, MAX_OBJECT_COUNT - 1)) { proc_playable_op(); }
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
	if(CharacterMove) {
		//If CharacterMove == TRUE, playable character will follow mouse
		//Set player move speed
		double cx, cy;
		getlocalcoord((uint16_t)PlayingCharacterID, &cx, &cy);
		double dx, dy;
		dx = constrain_number(CursorX, cx - 100, cx + 100) - (cx - 100);
		dy = constrain_number(CursorY, cy - 100, cy + 100) - (cy - 100);
		//g_print("%f, %f, %f, %f\n", dx, dy, cx, cy);
		Gobjs[PlayingCharacterID].sx = scale_number(dx, 200, PlayerSpeed * 2) - PlayerSpeed;
		Gobjs[PlayingCharacterID].sy = PlayerSpeed - scale_number(dy, 200, PlayerSpeed * 2);
	} else {
		Gobjs[PlayingCharacterID].sx = 0;
		Gobjs[PlayingCharacterID].sy = 0;
	}
	//Set camera location to display playable character in the center of display.
	CameraX = (uint16_t)constrain_number(Gobjs[PlayingCharacterID].x - (WINDOW_WIDTH / 2.0), 0, MAP_WIDTH - WINDOW_WIDTH);
	CameraY = (uint16_t)constrain_number(Gobjs[PlayingCharacterID].y - (WINDOW_HEIGHT / 2.0), 0, MAP_HEIGHT - WINDOW_HEIGHT);
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
	if(ITEMPRICES[SelectingItemID] > Money) {
		return;
	}
	//Check for cooldown
	if(ItemCooldownTimers[SelectingItemID] != 0) {
		return;
	}
	//Buy facility or use item
	if(is_range(SelectingItemID, 0, 3) ) {
		if( buy_facility((uint8_t)SelectingItemID) == FALSE) {
			return; //Could not buy facility
		}
	} else if (SelectingItemID == 4) {
		if(PlayerSpeed < 4.0) {
			PlayerSpeed += 0.5;
		} else {
			showerrorstr(10);
			return;
		}
	}
	//If succeed, decrease money and set up Cooldown timer
	Money -= ITEMPRICES[SelectingItemID];
	ItemCooldownTimers[SelectingItemID] = ITEMCOOLDOWNS[SelectingItemID];
}

gboolean buy_facility(uint8_t fid) {
	//Try to buy facility (Key handler)
	if(!is_range(fid, 0, 3) ) {
		die("buy_facility(): bad fid passed.\n");
		return FALSE;
	}
	int8_t tid = FTIDS[fid];
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
				showerrorstr(0);
				return FALSE;
			}
		}
	}
	add_character((uint8_t)tid, mx, my);
	return TRUE;
}

//Select next item candidate
void select_next_item() {
	if(GameState == GAMESTATE_PLAYING || GameState == GAMESTATE_DEAD) {
		if(SelectingItemID < ITEM_COUNT - 1) {
			SelectingItemID++;
		} else {
			SelectingItemID = -1;
		}
	}
}

//Select previous item candidate
void select_prev_item() {
	if(GameState == GAMESTATE_PLAYING || GameState == GAMESTATE_DEAD) {
		if(SelectingItemID > -1) {
			SelectingItemID--;
		} else {
			SelectingItemID = ITEM_COUNT - 1;
		}
	}
}

void debug_add_character() {
	if(DebugMode) {
		double mx, my;
		local2map(CursorX, CursorY, &mx, &my);
		add_character(AddingTID, mx, my);
	}
}

void start_command_mode(gboolean c) {
	KeyFlags = 0;
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
			AddingTID = (uint8_t)i;
		} else {
			showerrorstr(1);
		}
	} else if(strcmp(CommandBuffer, "/reset") == 0) {
		reset_game();
	} else if(strcmp(CommandBuffer, "/jp") == 0) {
		LangID = LANGID_JP;
	} else if(strcmp(CommandBuffer, "/en") == 0) {
		LangID = LANGID_EN;
	} else if(memcmp(CommandBuffer, "/chfont ", 8) == 0) {
		//cairo_select_font_face(G, &CommandBuffer[8], CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		loadfont(&CommandBuffer[8]);
	} else {
		chat(CommandBuffer);
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
		if(CommandCursor < g_utf8_strlen(CommandBuffer, 65535)) {
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
			if(strlen(CommandBuffer) + strlen(s) + 1 <= sizeof(CommandBuffer)) {
				utf8_insertstring(CommandBuffer, s, (uint16_t)CommandCursor, sizeof(CommandBuffer));
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
	if(utf8_insertstring(CommandBuffer, cb, (uint16_t)CommandCursor, sizeof(CommandBuffer) ) == TRUE) {
		CommandCursor += (int16_t)g_utf8_strlen(cb, 65535);
	} else {
		g_print("main.c: clipboard_read_handler(): insert failed.\n");
	}
		CommandBufferMutex = FALSE;
	free(cb);
}

void reset_game() {
	PlayableInfo_t t;
	lookup_playable(CurrentPlayableCharacterID, &t);
	GameState = GAMESTATE_INITROUND;
	Money = 0;
	PlayerSpeed = 1.0;
	SelectingItemID = -1;
	CharacterMove = FALSE;
	CameraX = 0;
	CameraY = 0;
	//Initialize Cooldown Timer
	for(uint8_t i = 0; i < ITEM_COUNT; i++) {
		ItemCooldownTimers[i] = 0;
	}
	//Initialize GameObj Slots
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		Gobjs[i].tid = TID_NULL;
	}
	EarthID = add_character(TID_EARTH, 200, 200);
	add_character(TID_ENEMYBASE, MAP_WIDTH - 200, MAP_HEIGHT - 200);
	add_character(TID_ENEMYBASE, MAP_WIDTH - 200, MAP_HEIGHT - 500);
	add_character(TID_ENEMYBASE, MAP_WIDTH - 500, MAP_HEIGHT - 200);
	PlayingCharacterID = (int16_t)add_character(t.associatedtid, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
}

//Set object to aim the earth
void aim_earth(uint16_t i) {
	//Gobjs[i].aiming_target = (int16_t)find_earth();
		for(uint16_t j = 0; j < MAX_OBJECT_COUNT; j++) {
			if(Gobjs[j].tid == TID_EARTH) {
				Gobjs[i].aiming_target = (int16_t)j;
				break;
			}
		}
	set_speed_for_following(i);
}

void showerrorstr(uint8_t errorid) {
	RecentErrorId = (int8_t)errorid;
	ErrorShowTimer = ERROR_SHOW_TIMEOUT;
}
