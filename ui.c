/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

ui.c: gamescreen drawing
*/

#include "inc/zundagame.h"

#include <string.h>
#include <stdarg.h>
#include <time.h>

void draw_game_main();
void draw_cui();
void draw_info();
void draw_hotbar(double, double);
void draw_game_object(int32_t, LookupResult_t, double, double);
void draw_shapes(int32_t, double, double);
void draw_hpbar(double, double, double, double, double, double, uint32_t, uint32_t);
double drawstring_inwidth(double, double, char*, int32_t, int32_t);
void drawsubstring(double, double, char*, int32_t, int32_t);
int32_t drawstring_title(double, char*, int32_t);
void drawstringf(double, double, const char*, ...);
int32_t get_substring_width(char*, int32_t, int32_t);
int32_t shrink_substring(char*, int32_t, int32_t, int32_t, int32_t*);

extern GameObjs_t Gobjs[MAX_OBJECT_COUNT]; //Game Objects
extern int32_t CameraX, CameraY; //Camera Position
extern int32_t CommandCursor; //Command System Related
extern char CommandBuffer[BUFFER_SIZE];
extern int32_t ChatTimeout; //Remaining time to show chat
extern char ChatMessages[MAX_CHAT_COUNT][BUFFER_SIZE]; //Chat message buffer
extern int32_t CursorX, CursorY; //Current Cursor Pos
extern int32_t DebugMode;
extern gamestate_t GameState;
extern int32_t EarthID;
extern int32_t PlayingCharacterID; //Current Playable Character type ID
extern int32_t Money; //Current map money amount
extern int32_t CharacterMove; //Character Moving on/off
extern int32_t SelectingItemID;
extern const int32_t ITEMPRICES[ITEM_COUNT];
extern const int32_t ITEMIMGIDS[ITEM_COUNT];
extern char StatusTextBuffer[BUFFER_SIZE];
extern int32_t StatusShowTimer;
extern int32_t StateChangeTimer;
extern int32_t ItemCooldownTimers[ITEM_COUNT];
extern int32_t SkillCooldownTimers[SKILL_COUNT];
extern int32_t CurrentPlayableCharacterID; //Current Playable Character ID
extern keyflags_t KeyFlags;
extern int32_t SkillKeyState; //Keyflag for skill keys
extern int32_t MapTechnologyLevel; //Current map technology level (Increases when google placed)
extern int32_t MapEnergyLevel; //Current map energy level (Increases when powerplants placed)
extern int32_t MapRequiredEnergyLevel; //Current map required energy level (Increases when facilities placed except power plants)
extern smpstatus_t SMPStatus; //Server Connection Status
extern SMPPlayers_t SMPPlayerInfo[MAX_CLIENTS]; //SMP Remote player information
double DrawTime; //DrawTime (in milliseconds)
extern double GameTickTime;
extern int32_t AddingTID; //Debug mode appending object id
extern int32_t DebugStatType;
extern SMPProfile_t SMPProfs[];
extern int32_t SelectedSMPProf;
extern int32_t SpawnRemain;

//Paint event of window client, called for every 30mS
void game_paint() {
	//Prepare to measure
	#ifndef __WASM
		double tbefore = get_current_time_ms();
	#endif

	clear_screen();

	//Draw game
	draw_game_main(); //draw characters
	draw_cui(); //draw minecraft like cui
	draw_hotbar(IHOTBAR_XOFF, IHOTBAR_YOFF); //draw mc like hot bar
	draw_info(); //draw game information
	#ifndef __WASM
		//Calculate draw time (AVG)
		static int32_t dtmc = 0;
		static double sdt = 0;
		sdt += get_current_time_ms() - tbefore;
		dtmc++;
		//measure 10 times, calculate avg amd reset
		if(dtmc >= 10) {
			DrawTime = sdt / 10;
			sdt = 0;
			dtmc = 0;
		}
	#endif 
}

void draw_game_main() {
	//draw lol type skill helper
	if(is_range(PlayingCharacterID, 0, MAX_OBJECT_COUNT - 1) && is_range(CurrentPlayableCharacterID, 0, PLAYABLE_CHARACTERS_COUNT - 1) && is_range(SkillKeyState,0, SKILL_COUNT - 1) ) {
		PlayableInfo_t t;
		if(lookup_playable(CurrentPlayableCharacterID, &t) == -1) {
			return;
		}
		double x, y;
		getlocalcoord(PlayingCharacterID, &x, &y);
		chcolor(0x600000ff, 1);
		fillcircle(x, y, t.skillranges[SkillKeyState]);
	}
	//draw characters
	for(uint8_t z = 0; z < MAX_ZINDEX; z++) {
		for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
			if(Gobjs[i].tid == TID_NULL) {continue;} //Skip if slot is not used.
			LookupResult_t t;
			if(lookup(Gobjs[i].tid, &t) == -1) {
				return;
			}
			double x, y;
			getlocalcoord(i, &x, &y);
			if(z == 2) {
				//in zindex=2, draw mark if there are planet out of screen
				if(t.objecttype == UNITTYPE_FACILITY) {
					//Determine if object is out of screen
					double w = 0;
					double h = 0;
					double hw = 0;
					double hh = 0;
					if(is_range(Gobjs[i].imgid, 0, IMAGE_COUNT - 1) ) {
						get_image_size((uint8_t)Gobjs[i].imgid, &w, &h);
						hw = w / 2;
						hh = h / 2;
					}
					double tx = x - hw;
					double ty = y - hh;
					if(!is_range_number(tx, -w, WINDOW_WIDTH) || !is_range_number(ty, -h, WINDOW_HEIGHT) ) {
						//if object is out of range, show indicator
						double px = constrain_number(x, 0, WINDOW_WIDTH - 16);
						double py = constrain_number(y, 0, WINDOW_HEIGHT - 114);
						if(Gobjs[i].tid == TID_PIT) {
							drawimage(px, py, IMG_PIT_MAP_MARK); //pit icon
						} else if (Gobjs[i].tid == TID_EARTH) {
							drawimage(px, py, IMG_EARTH_ICO); //earth icon
						} else if(t.objecttype == UNITTYPE_FACILITY) {
							if(t.teamid == TEAMID_ALLY) {
								chcolor(COLOR_ALLY, 1);
								fillcircle(px + 8, py + 8, 16); //pit icon
							} else {
								chcolor(COLOR_ENEMY, 1);
								fillrect(px, py, 16, 16);
							}
						}
					}
				}
			}
			if(t.zindex != z) { continue; } //Draw objects in next loop if z index is differ
			if(is_range(Gobjs[i].imgid, 0, IMAGE_COUNT - 1) ) {
				//If imgid points existing image, draw image
				draw_game_object(i, t, x, y);
			} else if(Gobjs[i].imgid == -1) {
				//imgid == -1 means shapes
				draw_shapes(i, x, y);
			}
		}
	}
}

void draw_game_object(int32_t idx, LookupResult_t t, double x, double y) {
	if(!is_range(idx, 0, MAX_OBJECT_COUNT - 1)) {
		die("draw_game_object(): bad idx, how can you do that!?\n");
		return;
	}
	if(!is_range(Gobjs[idx].imgid, 0, IMAGE_COUNT - 1)) {
		die("draw_game_object(): bad Gobjs[idx].imgid, how can you do that!?\n");
		return;
	}
	double w;
	double h;
	get_image_size(Gobjs[idx].imgid, &w, &h);
	x = x - (w / 2.0);
	y = y - (h / 2.0);
	drawimage(x, y, Gobjs[idx].imgid);
	if(DebugMode) {
		chcolor(0xffffffff, 1);
		hollowrect(x, y, w, h);
		drawstringf(x, y, "ID=%d", idx);
		chcolor(0x30ffffff, 1);
		fillcircle(x + (w / 2.0), y + (h / 2.0), Gobjs[idx].hitdiameter);
	}
	//Draw HP bar if their initial HP is not 0
	if(t.inithp != 0) {
		uint32_t cc = COLOR_ENEMY;
		//change fgcolor by team
		if(t.teamid == TEAMID_ALLY) {
			cc = COLOR_ALLY;
		}
		int32_t hpy = y - 7;
		//facility hpbar can be bottom of the object if they will be out of frame
		if(t.objecttype == UNITTYPE_FACILITY && hpy < 0) {
			hpy = y + h + 2;
		}
		draw_hpbar(x, hpy, w, 5, Gobjs[idx].hp, t.inithp, COLOR_TEXTCHAT, cc);
	}
	//In SMP: If the object is playable and from foreign client (SMP), draw name tag
	if(SMPStatus == NETWORK_LOGGEDIN && is_playable_character(Gobjs[idx].tid) ) {
		for(int32_t i = 0; i < MAX_CLIENTS; i++) {
			if(SMPPlayerInfo[i].cid != -1 && SMPPlayerInfo[i].playable_objid == idx) {
				drawstring_inwidth(x, y - 50, SMPPlayerInfo[i].usr, (int32_t)w, 1);
				break;
			}
		}
	}
	double bary = y + h + 2;
	if(is_playable_character(Gobjs[idx].tid) ) {
		//Timer bar for skill
		for(uint8_t i = 0; i < SKILL_COUNT; i++) {
			if(Gobjs[idx].timers[i + 1] != 0) {
				PlayableInfo_t plinfo;
				if(lookup_playable(CurrentPlayableCharacterID, &plinfo) == -1) {
					return;
				}
				draw_hpbar(x, bary, w, 5, Gobjs[idx].timers[i + 1], plinfo.skillinittimers[i], COLOR_ENEMY, COLOR_TEXTCHAT);
				bary += 7;
			}
		}
	}
}

void draw_shapes(int32_t idx, double x, double y) {
	if(!is_range(idx, 0, MAX_OBJECT_COUNT - 1)) {
		die("draw_game_object(): bad idx, how can you do that!?\n");
		return;
	}
	obj_type_t tid = Gobjs[idx].tid;
	if(DebugMode) {
		chcolor(0xffffffff, 1);
		drawstringf(x, y, "(Shape) ID=%d", idx);
	}
	//draw shapes
	double d = 0;
	if(DebugMode) {
		if(tid == TID_EARTH) {
			d = EARTH_RADAR_DIAM;
		} else if(tid == TID_ENEMYBASE) {
			d = ENEMYBASE_RADAR_DIAM;
		} else if(tid == TID_PIT) {
			d = PIT_RADAR_DIAM;
		} else if(tid == TID_FORT) {
			d = FORT_RADAR_DIAM;
		} else if(tid == TID_ZUNDAMON2) {
			d = ZUNDAMON2_RADAR_DIAM;
		} else if(tid == TID_ZUNDAMON3) {
			d = ZUNDAMON3_RADAR_DIAM;
		} else if(tid == TID_KUMO9_X24_ROBOT) {
			d = PLAYABLE_AUTOMACHINEGUN_DIAM;
		}
	}
	if(tid == TID_ALLYEXPLOSION || tid == TID_ENEMYEXPLOSION || tid == TID_EXPLOSION) {
		d = Gobjs[idx].hitdiameter;
	}
	if(tid == TID_ALLYEXPLOSION) {
		chcolor(0x7000A0FF, 1);
	} else if(tid == TID_ENEMYEXPLOSION) {
		chcolor(0x70ffa000, 1);
	} else if(tid == TID_EXPLOSION) {
		chcolor(0x70ff0000, 1);
	}
	if(d != 0) {
		//Draw circle
		fillcircle(x, y, d);
	}
	if(tid == TID_ENEMYZUNDALASER || tid == TID_KUMO9_X24_LASER || tid == TID_KUMO9_X24_PCANNON) {
		//Draw laser obj
		uint32_t lasercolour = 0xc07f00ff;
		double laserwidth = 5;
		if(tid == TID_ENEMYZUNDALASER) {
			//TID_ENEMYZUNDALASER: green, width: 20
			lasercolour = 0xa000ff00;
			laserwidth = 20;
		} else if(tid == TID_KUMO9_X24_PCANNON) {
			//TID_KUMO9_X24_PCANNON: white, width gets thinner if time ticks.
			lasercolour = COLOR_KUMO9_X24_PCANNON;
			laserwidth = scale_number(Gobjs[idx].timeout, 20, 30);
		}
		if(is_range(Gobjs[idx].aiming_target, 0, MAX_OBJECT_COUNT - 1)) {
			chcolor(lasercolour, 1);
			double sx = x;
			double sy = y;
			//if(is_range(Gobjs[idx].parentid, 0, MAX_OBJECT_COUNT - 1) ) {
			//	getlocalcoord( (uint16_t)Gobjs[idx].parentid, &sx, &sy);
			//}
			double tx;
			double ty;
			getlocalcoord(Gobjs[idx].aiming_target, &tx, &ty);
			drawline(sx, sy, tx, ty, laserwidth);
			chcolor(0xa0ff0000, 1);
			fillcircle(tx, ty, 50);
		} else {
			//printf("main.c: draw_game_main(): WARNING: ID%d is laser but it has bad target id!\n", idx);
		}
	}
}

void draw_cui() {
	//draw minecraft like cui
	int32_t fh = get_font_height();
	double yoff = IHOTBAR_YOFF - fh - 12; //text area pos

	//Show F3
	chcolor(COLOR_TEXTCMD, 1);
	if(DebugStatType == 1) {
		//Calculate obj count
		int32_t objc = 0;
		for(int32_t i = 0; i < MAX_OBJECT_COUNT; i++) {
			if(Gobjs[i].tid != TID_NULL) {
				objc++;
			}
		}
		drawstringf(0, 0, "System: %.2f %.2f (%.2f%%) %.2f%%", DrawTime, GameTickTime, GameTickTime / 10.0, (double)objc / (double)MAX_OBJECT_COUNT); //System Statics
	} else if(DebugStatType == 2) {
		drawstringf(0, 0, "Input: (%d, %d) (%d, %d) 0x%02x", CameraX, CameraY, CursorX, CursorY, KeyFlags); //Input Statics
	} else if(DebugStatType == 3) {
		drawstringf(0, 0, "Network: %d %d", SMPStatus, SelectedSMPProf);
	}

	//Show command input window if command mode
	if(CommandCursor != -1) {
		//Shrink string to fit in screen
		uint16_t sp = 0;
		int32_t w = WINDOW_WIDTH + 10;
		int32_t l = utf8_strlen(CommandBuffer);
		while(w > WINDOW_WIDTH - 5) {
			w = get_substring_width(CommandBuffer, sp, CommandCursor);
			if(w <= WINDOW_WIDTH - 5 || sp >= l - 1) { break; }
			sp++;
		}

		//Show command buffer
		chcolor(COLOR_TEXTBG, 1); //White (70% transparent)
		fillrect(0, yoff, WINDOW_WIDTH, fh + 2);
		chcolor(COLOR_TEXTCMD, 1); //Green
		drawsubstring(0, yoff, CommandBuffer, sp, 65535);
		drawline(w, yoff, w , yoff + fh + 2, 1);
	}

	//Show message window if there are message
	if(CommandCursor != -1 || ChatTimeout != 0) {
		chcolor(COLOR_TEXTCHAT, 1);
		double ty = (double) fh * 2;
		for(uint8_t i = 0; i < MAX_CHAT_COUNT; i++) {
			if(strlen(ChatMessages[i]) != 0) {
				//print(m)
				ty = drawstring_inwidth(0, ty, ChatMessages[i], WINDOW_WIDTH - 5, 1);
			}
		}
	}
}

void draw_info() {
	//Draw Error Message if there are error
	if(StatusShowTimer != 0) {
		/*if(!is_range(RecentErrorId, 0, MAX_STRINGS - 1) ) {
			die("draw_info(): bad RecentErrorId!\n");
			return;
		}*/
		chcolor(COLOR_TEXTERROR, 1);
		drawstring_title(WINDOW_WIDTH / 2, StatusTextBuffer, FONT_DEFAULT_SIZE);
	}

	//Draw title Message when GAMESTATE != PLAYING
	if(GameState != GAMESTATE_PLAYING) {
		chcolor(COLOR_TEXTCMD, 1);
		int32_t r;
		if(GameState == GAMESTATE_INITROUND) {
			r = drawstring_title(100, (char*)getlocalizedstring(2), 48);
			drawstring_title(120 + r, (char*)getlocalizedstring(3), FONT_DEFAULT_SIZE);
			
		} else if(GameState == GAMESTATE_GAMEOVER) {
			r = drawstring_title(100, (char*)getlocalizedstring(6), 48);
			drawstring_title(120 + r, (char*)getlocalizedstring(7), FONT_DEFAULT_SIZE);
			
		} else if(GameState == GAMESTATE_GAMECLEAR) {
			r = drawstring_title(100, (char*)getlocalizedstring(8), 48);
			drawstring_title(120 + r, (char*)getlocalizedstring(9), FONT_DEFAULT_SIZE);
			
		} else if(GameState == GAMESTATE_DEAD) {
			r = drawstring_title(100, (char*)getlocalizedstring(4), 48);
			drawstring_title(120 + r, (char*)getlocalizedstring(5), FONT_DEFAULT_SIZE);
			
		}
	}

	//Show game status
	//Show icons
	drawimage(STATUS_XOFF, STATUS_YOFF, IMG_STAT_MONEY_ICO); //Money ico
	drawimage(STATUS_XOFF, STATUS_YOFF + 16, IMG_STAT_TECH_ICO); //Tech icon
	drawimage(STATUS_XOFF + 80, STATUS_YOFF, IMG_EARTH_ICO); //Earth Icon
	//Show Money val
	chcolor(COLOR_TEXTCMD, 1);
	drawstringf(STATUS_XOFF + 16, STATUS_YOFF, "%d", Money);
	//Show MapTechnologyLevel
	drawstringf(STATUS_XOFF + 16, STATUS_YOFF + 16, "%d", MapTechnologyLevel);
	//Show Energy Level
	//Show Energy Icon
	uint8_t tiid = IMG_STAT_ENERGY_GOOD; //battery icon index, if energy level is inefficient, change icon
	uint32_t ttxtclr = COLOR_TEXTCMD;
	if(MapRequiredEnergyLevel > MapEnergyLevel) {
		tiid = IMG_STAT_ENERGY_BAD;
		ttxtclr = COLOR_TEXTERROR;
	}
	drawimage(STATUS_XOFF, STATUS_YOFF + 32, tiid);
	//Show current Energy level
	chcolor(ttxtclr, 1);
	drawstringf(STATUS_XOFF + 16, STATUS_YOFF + 32, "%d/%d", MapRequiredEnergyLevel, MapEnergyLevel);
	//Show Earth HP
	LookupResult_t t;
	if(is_range(EarthID, 0, MAX_OBJECT_COUNT - 1) && Gobjs[EarthID].tid == TID_EARTH && lookup(Gobjs[EarthID].tid, &t) != -1) {
		int32_t earthhp = Gobjs[EarthID].hp;
		chcolor(COLOR_TEXTCMD, 1);
		drawstringf(STATUS_XOFF + 96, STATUS_YOFF, "%d/%d", earthhp, t.inithp);
	}

	//Show status about playable character
	if(is_range(PlayingCharacterID, 0, MAX_OBJECT_COUNT - 1) && is_playable_character(Gobjs[PlayingCharacterID].tid) && lookup(Gobjs[PlayingCharacterID].tid, &t) != -1) {
		int32_t myhp = Gobjs[PlayingCharacterID].hp;
		if(CharacterMove) {
			drawimage(STATUS_XOFF + 80, STATUS_YOFF + 16, 20); //Show mouse icon
		} else {
			drawimage(STATUS_XOFF + 80, STATUS_YOFF + 16, 35); //Show keyboard icon
		}
		chcolor(COLOR_TEXTCMD, 1);
		drawstringf(STATUS_XOFF + 96, STATUS_YOFF + 16, "%d/%d x %d", myhp, t.inithp, SpawnRemain);
	} else {
		chcolor(COLOR_TEXTERROR, 1);
		drawstringf(STATUS_XOFF + 96, STATUS_YOFF + 16, "%d", StateChangeTimer);
	}
}

void draw_hotbar(double offsx, double offsy) {
	//Show item candidates and skill
	PlayableInfo_t plinf;
	if(lookup_playable(CurrentPlayableCharacterID, &plinf) == -1) {
		return;
	}
	for(uint8_t i = 0; i < ITEM_COUNT + SKILL_COUNT; i++) {
		double tx = offsx + (i * 50);
		int32_t itemlabel = 0;
		int32_t itemdisabled = 0;
		//Draw item background
		if(SelectingItemID == i) {
			//When item is selected, change backcolor and show item description
			chcolor(COLOR_TEXTCMD, 1);
			drawstring_inwidth(offsx, offsy + 52, (char*)getlocalizeditemdesc(SelectingItemID), WINDOW_WIDTH / 2, COLOR_TEXTBG);
			chcolor(COLOR_TEXTCHAT, 1);
		} else {
			chcolor(COLOR_TEXTBG, 1);
		}
		fillrect(tx, offsy, 48, 48);

		if(i < ITEM_COUNT) {
			//Show Item Image (hotbar)
			drawimage_scale(tx, offsy, 48, 48, ITEMIMGIDS[i]);
			//If item is not available, make text red.
			if(ItemCooldownTimers[i] != 0 || Money < ITEMPRICES[i]) {
				itemdisabled = 1;
			}
			if(ItemCooldownTimers[i] != 0) {
				itemlabel = ItemCooldownTimers[i];
			} else {
				itemlabel = ITEMPRICES[i];
			}
		} else {
			//Draw Skills Icon
			int32_t j = i - ITEM_COUNT;
			drawimage_scale(tx, offsy + 2, 48, 48, plinf.skillimageids[j]);
			if(SkillCooldownTimers[j] != 0) {
				itemdisabled = 1;
				itemlabel = SkillCooldownTimers[j];
			} else {
				//Draw QWE Label
				const char* L[] = {"Q", "W", "E"};
				chcolor(COLOR_TEXTCMD, 1);
				drawstring(tx + 2, offsy + 2 + 24, L[j]);
			}
		}
	
		//draw unusable icon if unavailable
		if(itemdisabled) {
			drawimage(tx, offsy, IMG_ITEM_UNUSABLE);
			chcolor(COLOR_TEXTERROR, 1);
		} else {
			chcolor(COLOR_TEXTCMD, 1);
		}
		if(!(i >= ITEM_COUNT && !itemdisabled) ) {
			//already drawn: skill label when skill usable
			drawstringf(tx + 2, offsy + 2 + 24, "%d", itemlabel);
		}
	}
}

//Draw hp bar at x, y with width w and height h, chp is current hp, fhp is full hp,
//colorbg is background color, colorfg is foreground color.
void draw_hpbar(double x, double y, double w, double h, double chp, double fhp, uint32_t colorbg, uint32_t colorfg) {
	chcolor(colorbg, 1);
	fillrect(x, y, w, h);
	double bw = scale_number(chp, fhp, w);
	chcolor(colorfg, 1);
	fillrect(x, y, bw, h);
}

//draw substring ctx (index sp to ed) in pos x, y
void drawsubstring(double x, double y, char* ctx, int32_t st, int32_t ed) {
	char b[BUFFER_SIZE];
	utf8_substring(ctx, st, ed, b, sizeof(b));
	drawstring(x, y, b);
}

void drawstringf(double x, double y, const char *p, ...) {
	char b[BUFFER_SIZE];
	va_list v;
	va_start(v, p);
	ssize_t r = vsnprintf(b, sizeof(b), p, v);
	va_end(v);
	//Safety check
	if(r >= sizeof(b) || r == -1) {
		die("gutil.c: drawstringf() failed: formatted string not null terminated, input format string too long?\n");
		return;
	}
	//Print
	drawstring(x, y, b);
}

//draw string and let it fit in width wid using newline. if bg is 1, string will have rectangular background
double drawstring_inwidth(double x, double y, char* ctx, int32_t wid, int32_t bg) {
	double ty = y;
	int32_t cp = 0;
	int32_t ch = get_font_height();
	int32_t ml = utf8_strlen(ctx);
	while(cp < ml) {
		//Write a adjusted string to fit in wid
		int32_t a;
		int32_t t = shrink_substring(ctx, wid, cp, ml, &a);
		if(bg) {
			chcolor(COLOR_TEXTBG, 0);
			fillrect(x, ty, a, ch + 5);
			restore_color();
		}
		drawsubstring(x, ty, ctx, cp, t);
	//	g_print("Substring: %d %d\n", cp, t);
		cp = t;
		ty += ch + 5;
	}
	return ty;
}

int32_t drawstring_title(double y, char* ctx, int32_t s) {
	//cairo_set_font_size(G, s);
	set_font_size(s);
	int32_t w = get_string_width(ctx);
	int32_t h = get_font_height();
	double x = (WINDOW_WIDTH / 2) - (w / 2);
	chcolor(COLOR_TEXTBG, 0);
	fillrect(x, y, w, h);
	restore_color();
	drawstring(x, y, ctx);
	//cairo_set_font_size(G, FONT_DEFAULT_SIZE);
	set_font_size(FONT_DEFAULT_SIZE);
	return h;
}

//get_string_width but substring version (from index st to ed)
int32_t get_substring_width(char* ctx, int32_t st, int32_t ed) {
	char b[BUFFER_SIZE];
	utf8_substring(ctx, st, ed, b, sizeof(b) );
	return get_string_width(b);
}

//substring version of shrink_string()
int32_t shrink_substring(char *ctx, int32_t wid, int32_t st, int32_t ed, int32_t* widr) {
	int32_t l = constrain_i32(ed, 0, utf8_strlen(ctx) );
	int32_t w = wid + 10;
	//Delete one letter from ctx in each loops, continue until string width fits in wid
	while(l > 0) {
		w = get_substring_width(ctx, st, l);
		if(widr != NULL) {*widr = w;}
		if(w < wid || l <= 1) { return l; }
		l--;
		//g_print("%d %d\n", l, w);
	}
	return 1;
}


