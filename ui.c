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

//Image ID Definition for special purposes
#define IMG_ITEM_UNUSABLE 13 //Cross icon, this means item is unusable
#define IMG_EARTH_ICO 17 //Earth icon, represents the earth or its HP
#define IMG_STAT_MONEY_ICO 18 //Money Icon
#define IMG_MOUSE_ICO 20 //Mouse Icon
#define IMG_PIT_MAP_MARK 21 //pit map mark
#define IMG_ICO_IMGMISSING 22 //Image Missing icon
#define IMG_STAT_TECH_ICO 23 //Technology level icon
#define IMG_STAT_ENERGY_GOOD 31 //Energy icon (good)
#define IMG_STAT_ENERGY_BAD 32 //Energy icon (insufficient energy)
#define IMG_TITLE 35 //Title image id

//Colors
#define COLOR_TEXTBG 0x60000000 //Text background color (30% opaque black)
#define COLOR_TEXTCMD 0xff00ff00 //Command and Chat text color (Green)
#define COLOR_TEXTCHAT 0xffffffff //Chat text color
#define COLOR_TEXTERROR 0xffff0000 //Error text color: RED
#define COLOR_ENEMY 0xffff0000 //Enemy HP bar and marker color
#define COLOR_ALLY 0xff00ffa0 //Enemy HP bar and marker color
#define COLOR_UNUSABLE 0x60000000 //Gray out color
#define COLOR_KUMO9_X24_PCANNON 0xc0ffffff //kumo9 x24 pcannon color

//Coordinates
#define IHOTBAR_XOFF 5 //Item hotbar X offset
#define IHOTBAR_YOFF (WINDOW_HEIGHT - 100) //Item hotbar Y offset
#define STATUS_XOFF 0
#define STATUS_YOFF 0

#include "inc/zundagame.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>

void draw_game_main();
void draw_cui();
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
void draw_map_status();
void draw_locator();
void draw_image_center(int32_t);
void draw_title_screen();

extern int32_t ITEMCOOLDOWNS[ITEM_COUNT];
extern langid_t LangID;
extern GameObjs_t Gobjs[MAX_OBJECT_COUNT]; //Game Objects
extern int32_t CameraX, CameraY; //Camera Position
extern int32_t CommandCursor; //Command System Related
extern char CommandBuffer[BUFFER_SIZE];
extern int32_t ChatTimeout; //Remaining time to show chat
extern char ChatMessages[MAX_CHAT_COUNT][BUFFER_SIZE]; //Chat message buffer
extern int32_t CursorX, CursorY; //Current Cursor Pos
extern int32_t DebugMode;
extern gamestate_t GameState;
extern int32_t EarthID; //Gobjs[] ID of Earth
extern int32_t PlayingCharacterID; //Playable character object id
extern int32_t CurrentPlayableID; //Current Playable Character type ID
extern int32_t Money; //Current map money amount
extern int32_t CharacterMove; //Character Moving on/off
extern int32_t SelectingItemID;
extern int32_t SelectedMenuItem;
extern const int32_t ITEMPRICES[ITEM_COUNT];
extern const int32_t ITEMIMGIDS[ITEM_COUNT];
extern char StatusTextBuffer[BUFFER_SIZE];
extern int32_t StatusShowTimer;
extern int32_t StateChangeTimer;
extern int32_t ItemCooldownTimers[ITEM_COUNT];
extern int32_t SkillCooldownTimers[SKILL_COUNT];
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
extern int32_t LocatorType; //Ruler type id
extern int32_t SelectingMenuItem;
extern int32_t DifEnemyBaseDist;
extern int32_t DifEnemyBaseCount[4];
extern int32_t InitSpawnRemain;
extern double DifATKGain;
extern int32_t PlayableID;
extern int32_t SelectingHelpPage;

//Paint event of window client, called for every 30mS
void game_paint() {
	//Prepare to measure
	#ifndef __WASM
		double tbefore = get_current_time_ms();
	#endif
	
	clear_screen();

	//Draw game
	if(GameState != GAMESTATE_TITLE) {
		draw_locator(); //draw game background
	}
	draw_game_main(); //draw characters
	if(GameState != GAMESTATE_TITLE) {
		draw_hotbar(IHOTBAR_XOFF, IHOTBAR_YOFF); //draw mc like hot bar
		draw_map_status(); //draw money, techevel etc
	}
	draw_cui(); //draw chat and command window, title string, error
	if(GameState == GAMESTATE_TITLE) {
		if(SelectingHelpPage == -1) {
			draw_title_screen();
		} else {
			if(is_range(SelectingHelpPage, 0, HELP_PAGE_MAX - 1) ) {
				draw_image_center(getlocalizedhelpimgid(SelectingHelpPage) );
			}
		}
	}

	if(GameState != GAMESTATE_TITLE && (KeyFlags & KEY_HELP) ) {
		draw_image_center(getlocalizedhelpimgid(3) );
	}

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
	double w, h, tx, ty;
	get_image_size(Gobjs[idx].imgid, &w, &h);
	tx = x - (w / 2.0);
	ty = y - (h / 2.0);
	drawimage(tx, ty, Gobjs[idx].imgid);
	if(DebugMode) {
		chcolor(0xffffffff, 1);
		hollowrect(tx, ty, w, h);
		drawstringf(tx, ty, "ID=%d", idx);
		chcolor(0x30ffffff, 1);
		fillcircle(x, y, Gobjs[idx].hitdiameter);
	}
	//Draw HP bar if their initial HP is not 0
	if(t.inithp != 0) {
		uint32_t cc = COLOR_ENEMY;
		//change fgcolor by team
		if(t.teamid == TEAMID_ALLY) {
			cc = COLOR_ALLY;
		}
		double hpy = ty - 7;
		//facility hpbar can be bottom of the object if they will be out of frame
		if(t.objecttype == UNITTYPE_FACILITY && hpy < 0) {
			hpy = ty + h + 2;
		}
		draw_hpbar(tx, hpy, w, 5, Gobjs[idx].hp, t.inithp, COLOR_TEXTCHAT, cc);
	}
	//In SMP: If the object is playable and from foreign client (SMP), draw name tag
	if(SMPStatus == NETWORK_LOGGEDIN && is_playable_character(Gobjs[idx].tid) ) {
		for(int32_t i = 0; i < MAX_CLIENTS; i++) {
			if(SMPPlayerInfo[i].cid != -1 && SMPPlayerInfo[i].playable_objid == idx) {
				drawstring_inwidth(tx, ty - 50, SMPPlayerInfo[i].usr, (int32_t)w, 1);
				break;
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

//draw title screen
void draw_title_screen() {
	//Draw title image
	double w, h, y, x;
	y = 100;
	get_image_size(IMG_TITLE, &w, &h);
	x = (WINDOW_WIDTH / 2.0) - (w / 2.0);
	drawimage(x, y - (h / 2.0), IMG_TITLE);
	y += (h / 2.0) + 50;
	drawstring_title(y, getlocalizedstring(12), FONT_DEFAULT_SIZE);
	y += 50;

	//Draw menu
	for(int i = 0; i < MAX_MENU_STRINGS; i++) {
		//Draw menu item
		const char *t = getlocalizedmenustring(i);
		double fh = get_font_height();
		if(SelectingMenuItem == i) {
			chcolor(0x60ffffff, 1);
			fillrect(x, y, w, fh);
		}
		chcolor(COLOR_TEXTCMD, 1);
		drawstring(x, y, t);

		//Draw settings for setting item
		char st[BUFFER_SIZE] = "";
		if(i == 2) {
			snprintf(st, BUFFER_SIZE - 1, "%.3lf", DifATKGain);
		} else if(i == 3) {
			snprintf(st, BUFFER_SIZE - 1, "%d", DifEnemyBaseDist);
		} else if(i == 4) {
			snprintf(st, BUFFER_SIZE - 1, "%d", DifEnemyBaseCount[0]);
		} else if(i == 5) {
			snprintf(st, BUFFER_SIZE - 1, "%d", DifEnemyBaseCount[1]);
		} else if(i == 6) {
			snprintf(st, BUFFER_SIZE - 1, "%d", DifEnemyBaseCount[2]);
		} else if(i == 7) {
			if(InitSpawnRemain == -1) {
				strcpy(st, getlocalizedstring(11) );
			} else {
				snprintf(st, BUFFER_SIZE - 1, "%d", InitSpawnRemain);
			}
		} else if(i == 8) {
			snprintf(st, BUFFER_SIZE - 1, "%d", PlayableID);
		}
		st[BUFFER_SIZE - 1] = 0;
		if(strlen(st) != 0) {
			double tw = get_string_width(st);
			drawstring(x + w - tw, y, st);
		}
		y += fh;
	}
	if(is_range(PlayableID, 0, PLAYABLE_CHARACTERS_COUNT - 1) ) {
		PlayableInfo_t plinf;
		lookup_playable(PlayableID, &plinf);
		drawimage(x, y, plinf.portraitimgid);
	}
}

void draw_locator() {
	//draw ruler or grid
	chcolor(0x4000ff00, 1);
	if(LocatorType == 1) {
		//calculate first line coordinate
		int32_t cx = (int32_t)floor(CameraX / 100) + 1; 
		int32_t cy = (int32_t)floor(CameraY / 100) + 1;
		double tx = cx * 100; //first x coordinate (double of 100) after CameraX
		double ty = cy * 100; //first y coordinate (double of 100) after CameraY
		double lx, ly;
		map2local(tx, ty, &lx, &ly);
		//xgrid
		while(lx < WINDOW_WIDTH) {
			drawline(lx, 0, lx, WINDOW_HEIGHT, 1);
			drawstringf(lx, 0, "%d", cx);
			lx += 100;
			cx++;
		}
		//y-grid
		while(ly > 0) {
			drawline(0, ly, WINDOW_WIDTH, ly, 1);
			drawstringf(0, ly, "%d", cy);
			ly -= 100;
			cy++;
		}
	} else if(LocatorType == 2) {
		//x-ruler (show lines where x is double of 10, make line longer if it is double of 50)
		int32_t cx = (int32_t)floor( (CameraX + (WINDOW_WIDTH * 0.25) ) / 25.0);
		int32_t cy = (int32_t)floor( (CameraY + (WINDOW_HEIGHT * 0.25) ) / 25.0);
		double tx = cx * 25.0;
		double ty = cy * 25.0;
		double lx, ly;
		map2local(tx, ty, &lx, &ly);
		//x-ruler
		while(lx < (WINDOW_WIDTH * 0.75) ) {
			double ll = 5;
			if(cx % 4 == 0) {
				ll = 20;
				drawstringf(lx, 50, "%d", (cx * 25) / 100);
			}
			drawline(lx, 20, lx, ll + 20, 1);
			lx += 25;
			cx++;
		}
		//y-ruler
		while(ly > (WINDOW_HEIGHT * 0.25) ) {
			double ll = 5;
			if(cy % 4 == 0) {
				ll = 20;
				drawstringf(20, ly, "%d", (cy * 25) / 100);
			}
			drawline(20, ly, ll + 20, ly, 1);
			ly -= 25;
			cy++;
		}
	} else if(LocatorType == 3) {
		//X grid
		int32_t cx = (int32_t)floor(CameraX / 100.0) + 1;
		int32_t cy = (int32_t)floor(CameraY / 100.0) + 1;
		double tx = cx * 100.0;
		double ty = cy * 100.0;
		double lx, ly;
		map2local(tx, ty, &lx, &ly);
		while(ly > 0) {
			double llx = lx;
			int32_t llcx = cx;
			while(llx < WINDOW_WIDTH) {
				drawline(llx - 5, ly - 5, llx + 5, ly + 5, 1);
				drawline(llx - 5, ly + 5, llx + 5, ly - 5, 1);
				drawstringf(llx, ly + 5, "%02d%02d", llcx, cy);
				llcx++;
				llx += 100;
			}
			cy++;
			ly -= 100;
		}
	}
}

void draw_cui() {
	//draw minecraft like cui
	int32_t fh = get_font_height();
	double yoff = IHOTBAR_YOFF - fh - 12; //text area pos

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
		double ty = (double) fh;
		for(uint8_t i = 0; i < MAX_CHAT_COUNT; i++) {
			if(strlen(ChatMessages[i]) != 0) {
				//print(m)
				ty = drawstring_inwidth(0, ty, ChatMessages[i], WINDOW_WIDTH - 5, 1);
			}
		}
	}

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

	//Show F3
	chcolor(COLOR_TEXTCMD, 1);
	const int32_t DEBUG_XOFF = (WINDOW_WIDTH / 2), DEBUG_YOFF = 0;
	if(DebugStatType == 1) {
		//Calculate obj count
		int32_t objc = 0;
		for(int32_t i = 0; i < MAX_OBJECT_COUNT; i++) {
			if(Gobjs[i].tid != TID_NULL) {
				objc++;
			}
		}
		drawstringf(DEBUG_XOFF, DEBUG_YOFF, "System: %.2f %.2f (%.2f%%) %.2f%%", DrawTime, GameTickTime, GameTickTime / 10.0, (double)objc / (double)MAX_OBJECT_COUNT); //System Statics
	} else if(DebugStatType == 2) {
		drawstringf(DEBUG_XOFF, DEBUG_YOFF, "Input: (%d, %d) (%d, %d) 0x%02x", CameraX, CameraY, CursorX, CursorY, KeyFlags); //Input Statics
	} else if(DebugStatType == 3) {
		drawstringf(DEBUG_XOFF, DEBUG_YOFF, "Network: %d %d", SMPStatus, SelectedSMPProf);
	}

}

void draw_image_center(int32_t imgid) {
	//Calculate img offset to show
	double iw, ih;
	get_image_size(imgid, &iw, &ih);
	drawimage( (WINDOW_WIDTH / 2) - (iw / 2), (WINDOW_HEIGHT / 2) - (ih / 2), imgid);
}

void draw_map_status() {
	//Show game status
	//Show icons
	drawimage(STATUS_XOFF, STATUS_YOFF, IMG_STAT_MONEY_ICO); //Money ico
	drawimage(STATUS_XOFF + 50, STATUS_YOFF, IMG_STAT_TECH_ICO); //Tech icon
	drawimage(STATUS_XOFF + 80, STATUS_YOFF, IMG_EARTH_ICO); //Earth Icon
	//Show Money val
	chcolor(COLOR_TEXTCMD, 1);
	drawstringf(STATUS_XOFF + 16, STATUS_YOFF, "%d", Money);
	//Show MapTechnologyLevel
	drawstringf(STATUS_XOFF + 66, STATUS_YOFF, "%d", MapTechnologyLevel);
	//Show Energy Level
	//Show Energy Icon
	uint8_t tiid = IMG_STAT_ENERGY_GOOD; //battery icon index, if energy level is inefficient, change icon
	uint32_t ttxtclr = COLOR_TEXTCMD;
	if(MapRequiredEnergyLevel > MapEnergyLevel) {
		tiid = IMG_STAT_ENERGY_BAD;
		ttxtclr = COLOR_TEXTERROR;
	}
	drawimage(STATUS_XOFF + 150, STATUS_YOFF, tiid);
	//text
	chcolor(ttxtclr, 1);
	drawstringf(STATUS_XOFF + 166, STATUS_YOFF, "%d/%d", MapRequiredEnergyLevel, MapEnergyLevel);

	//Show Earth HP
	LookupResult_t t;
	if(is_range(EarthID, 0, MAX_OBJECT_COUNT - 1) && Gobjs[EarthID].tid == TID_EARTH && lookup(Gobjs[EarthID].tid, &t) != -1) {
		chcolor(COLOR_TEXTCMD, 1);
		drawstringf(STATUS_XOFF + 96, STATUS_YOFF, "%.1lf%%", (Gobjs[EarthID].hp / t.inithp) * 100);
	}

	//Show timer if player dead
	if(GameState == GAMESTATE_DEAD) {
		drawimage(STATUS_XOFF + 250, STATUS_YOFF, 19);
		chcolor(COLOR_TEXTERROR, 1);
		drawstringf(STATUS_XOFF + 266, STATUS_YOFF, "%d", StateChangeTimer);
	}
}

void draw_hotbar(double offsx, double offsy) {
	//Show item candidates and skill
	PlayableInfo_t plinf;
	if(lookup_playable(CurrentPlayableID, &plinf) == -1) {
		return;
	}
	for(uint8_t i = 0; i < ITEM_COUNT + SKILL_COUNT; i++) {
		double tx = offsx + (i * 50);
		int32_t itemdisabled = 0;
		int32_t fullpbar;
		int32_t nowpbar;
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
			fullpbar = ITEMCOOLDOWNS[i];
			nowpbar = ItemCooldownTimers[i];
			//Show Item Image (hotbar)
			drawimage_scale(tx, offsy, 48, 48, ITEMIMGIDS[i]);
			//If item is not available, make text red.
			if(ItemCooldownTimers[i] != 0 || Money < ITEMPRICES[i]) {
				itemdisabled = 1;
				chcolor(COLOR_TEXTERROR, 1);	
			} else {
				chcolor(COLOR_TEXTCMD, 1);
			}
			drawstringf(tx + 2, offsy + 2 + 24, "%d", ITEMPRICES[i]);
		} else {
			//Draw Skills Icon
			int32_t j = i - ITEM_COUNT;
			fullpbar = plinf.skillcooldowns[j];
			nowpbar = SkillCooldownTimers[j];
			drawimage_scale(tx, offsy + 2, 48, 48, plinf.skillimageids[j]);
			if(SkillCooldownTimers[j] != 0) {
				itemdisabled = 1;
				chcolor(COLOR_TEXTERROR, 1);
			} else {
				chcolor(COLOR_TEXTCMD, 1);
			}
			//Draw QWE Label
			char* L[] = {"Q", "W", "E"};
			drawstring(tx + 2, offsy + 2 + 24, L[j]);
			//Draw skill timer
			if(is_range(PlayingCharacterID, 0, MAX_OBJECT_COUNT - 1) ) {
				int32_t t = Gobjs[PlayingCharacterID].timers[j + 1];
				if(t != 0) {
					double v = scale_number( (double)t, (double)plinf.skillinittimers[j], 192);
					chcolor(COLOR_TEXTCHAT, 1);
					drawline(tx + 24, offsy + 2, tx + 24 + constrain_number(v, 0, 24), offsy + 2, 2); //upper middle -> upper right
					if(v >= 24) {
						drawline(tx + 48, offsy + 2, tx + 48, offsy + 2 + constrain_number(v, 24, 72) - 24, 2); //upper right -> lower right
					}
					if(v >= 72) {
						drawline(tx + 48, offsy + 48 + 2, tx + 48 - (constrain_number(v, 72, 120) - 72 ), offsy + 48 + 2, 2); //lower right -> lower left
					}
					if(v >= 120) {
						drawline(tx, offsy + 48 + 2, tx, offsy + 48 + 2 - (constrain_number(v, 120, 168) - 120), 2); //lower left -> upper left
					}
					if(v >= 168) {
						drawline(tx, offsy + 2, tx + (constrain_number(v, 168, 192) - 168), offsy + 2, 2); //upper left -> upper middle
					}
				}
			}
		}
		
		//if item/skill is in cd, draw indicator
		if(nowpbar != 0) {
			chcolor(0xa0ff0000, 1);
			fillrect(tx, offsy + 2, 48, scale_number(nowpbar, fullpbar, 48) );
		}

		//draw unusable icon if unavailable
		if(itemdisabled) {
			drawimage(tx, offsy, IMG_ITEM_UNUSABLE);
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
		die("ui.c: drawstringf() failed: formatted string not null terminated, input format string too long?\n");
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


