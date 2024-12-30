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

#include "main.h"

#include <string.h>
#include <cairo/cairo.h>

//ui.c
void draw_game_main();
void draw_cui();
void draw_info();
void draw_mchotbar(double, double);
void draw_lolhotbar(double, double);
void draw_game_object(int32_t, LookupResult_t, double, double);
void draw_shapes(int32_t, LookupResult_t, double, double);

extern cairo_t* G; //Gamescreen cairo context
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
//Paint event of window client, called for every 30mS
void game_paint() {
	//Prepare to measure
	static int32_t dtmc = 0;
	static double sdt = 0;
	double beforetime = get_current_time_ms();

	//Clear screen
	cairo_set_source_rgb(G, 0, 0, 0);
	cairo_paint(G);

	//Draw game
	draw_game_main(); //draw characters
	draw_cui(); //draw minecraft like cui
	draw_mchotbar(IHOTBAR_XOFF, IHOTBAR_YOFF); //draw mc like hot bar
	draw_lolhotbar(LHOTBAR_XOFF, LHOTBAR_YOFF); //draw lol like hotbar
	draw_info(); //draw game information

	//Calculate draw time (AVG)
	sdt += get_current_time_ms() - beforetime;
	dtmc++;
	//measure 10 times, calculate avg amd reset
	if(dtmc >= 10) {
		DrawTime = sdt / 10;
		sdt = 0;
		dtmc = 0;
	} 
}

void draw_game_main() {
	//draw lol type skill helper
	if(is_range(PlayingCharacterID, 0, MAX_OBJECT_COUNT - 1) && is_range(CurrentPlayableCharacterID, 0, PLAYABLE_CHARACTERS_COUNT - 1) && is_range(SkillKeyState,0, SKILL_COUNT - 1) ) {
		PlayableInfo_t t;
		lookup_playable(CurrentPlayableCharacterID, &t);
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
			lookup(Gobjs[i].tid, &t);
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
						//object is out of range
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
				draw_shapes(i, t, x, y);
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
		draw_hpbar(x, y - 7, w, 5, Gobjs[idx].hp, t.inithp, COLOR_TEXTCHAT, cc);
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
				lookup_playable(CurrentPlayableCharacterID, &plinfo);
				draw_hpbar(x, bary, w, 5, Gobjs[idx].timers[i + 1], plinfo.skillinittimers[i], COLOR_ENEMY, COLOR_TEXTCHAT);
				bary += 7;
			}
		}
	}
}

void draw_shapes(int32_t idx, LookupResult_t t, double x, double y) {
	if(!is_range(idx, 0, MAX_OBJECT_COUNT - 1)) {
		die("draw_game_object(): bad idx, how can you do that!?\n");
		return;
	}
	if(DebugMode) {
		chcolor(0xffffffff, 1);
		drawstringf(x, y, "(Shape) ID=%d", idx);
	}
	//draw shapes
	double d = 0;
	if(DebugMode) {
		switch(Gobjs[idx].tid) {
			case TID_EARTH:
				d = EARTH_RADAR_DIAM;
			break;
			case TID_ENEMYBASE:
				d = ENEMYBASE_RADAR_DIAM;
			break;
			case TID_PIT:
				d = PIT_RADAR_DIAM;
			break;
			case TID_FORT:
				d = FORT_RADAR_DIAM;
			break;
			case TID_ZUNDAMON2:
				d = ZUNDAMON2_RADAR_DIAM;
			break;
			case TID_ZUNDAMON3:
				d = ZUNDAMON3_RADAR_DIAM;
			break;
			case TID_KUMO9_X24_ROBOT:
				d = PLAYABLE_AUTOMACHINEGUN_DIAM;
				break;
		}
	}
	if(Gobjs[idx].tid == TID_ALLYEXPLOSION || Gobjs[idx].tid == TID_ENEMYEXPLOSION || Gobjs[idx].tid == TID_EXPLOSION) {
		d = Gobjs[idx].hitdiameter;
	}
	switch(Gobjs[idx].tid) {
		case TID_ALLYEXPLOSION:
			chcolor(0x7000A0FF, 1);
		break;
		case TID_ENEMYEXPLOSION:
			chcolor(0x70ffa000, 1);
		break;
		case TID_EXPLOSION:
			chcolor(0x70ff0000, 1);
		break;
		case TID_KUMO9_X24_PCANNON:
			d = Gobjs[idx].timeout;
			chcolor(COLOR_KUMO9_X24_PCANNON, 1);
			break;
	}
	if(d != 0) {
		//Draw circle
		fillcircle(x, y, d);
	}
	if(Gobjs[idx].tid == TID_ENEMYZUNDALASER || Gobjs[idx].tid == TID_KUMO9_X24_LASER || (Gobjs[idx].tid == TID_KUMO9_X24_PCANNON && Gobjs[idx].timeout < 20) ) {
		//Draw laser obj
		uint32_t lasercolour = 0xc07f00ff;
		double laserwidth = 5;
		if(Gobjs[idx].tid == TID_ENEMYZUNDALASER) {
			//Apply EnemyZundaColourLaser if TID is TID_ENEMYZUNDALASER
			lasercolour = 0xa000ff00;
			laserwidth = 20;
		} else if(Gobjs[idx].tid == TID_KUMO9_X24_PCANNON) {
			lasercolour = COLOR_KUMO9_X24_PCANNON;
			laserwidth = 30;
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
		drawstringf(0, 0, "System: %.2f %.2f%% %.2f%%", DrawTime, GameTickTime / 10.0, (double)objc / (double)MAX_OBJECT_COUNT); //System Statics
	} else if(DebugStatType == 2) {
		drawstringf(0, 0, "Input: (%d, %d) (%d, %d) 0x%02x", CameraX, CameraY, CursorX, CursorY, KeyFlags); //Input Statics
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
	//Draw additional information
	int32_t fh = get_font_height();

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
		switch(GameState) {
		case GAMESTATE_INITROUND:
			r = drawstring_title(100, (char*)getlocalizedstring(2), 48);
			drawstring_title(120 + r, (char*)getlocalizedstring(3), FONT_DEFAULT_SIZE);
			break;
		case GAMESTATE_GAMEOVER:
			r = drawstring_title(100, (char*)getlocalizedstring(6), 48);
			drawstring_title(120 + r, (char*)getlocalizedstring(7), FONT_DEFAULT_SIZE);
			break;
		case GAMESTATE_GAMECLEAR:
			r = drawstring_title(100, (char*)getlocalizedstring(8), 48);
			drawstring_title(120 + r, (char*)getlocalizedstring(9), FONT_DEFAULT_SIZE);
			break;
		case GAMESTATE_DEAD:
			r = drawstring_title(100, (char*)getlocalizedstring(4), 48);
			drawstring_title(120 + r, (char*)getlocalizedstring(5), FONT_DEFAULT_SIZE);
			break;
		}
	}


	//Show game status
	//Show icons
	drawimage(STATUS_XOFF, IHOTBAR_YOFF, IMG_STAT_MONEY_ICO); //Money ico
	drawimage(STATUS_XOFF, IHOTBAR_YOFF + 16, IMG_STAT_TECH_ICO); //Tech icon
	//Show Money val
	chcolor(COLOR_TEXTCMD, 1);
	drawstringf(STATUS_XOFF + 16, IHOTBAR_YOFF, "%d", Money);
	//Show MapTechnologyLevel
	drawstringf(STATUS_XOFF + 16, IHOTBAR_YOFF + 16, "%d", MapTechnologyLevel);
	//Show Energy Level
	//Show Energy Icon
	uint8_t tiid = IMG_STAT_ENERGY_GOOD; //battery icon index, if energy level is inefficient, change icon
	uint32_t ttxtclr = COLOR_TEXTCMD;
	if(MapRequiredEnergyLevel > MapEnergyLevel) {
		tiid = IMG_STAT_ENERGY_BAD;
		ttxtclr = COLOR_TEXTERROR;
	}
	drawimage(STATUS_XOFF, IHOTBAR_YOFF + 32, tiid);
	//Show current Energy level
	chcolor(ttxtclr, 1);
	drawstringf(STATUS_XOFF + 16, IHOTBAR_YOFF + 32, "%d/%d", MapRequiredEnergyLevel, MapEnergyLevel);
}

void draw_lolhotbar(double offsx, double offsy) {
	//Show LOL like hotbar
	PlayableInfo_t t;
	lookup_playable(0, &t);
	chcolor(COLOR_TEXTBG, 1);
	fillrect(offsx, offsy, LHOTBAR_WIDTH, 100); //show bg
	//show portrait image
	drawimage(offsx + 2, offsy + 2, t.portraitimgid);
	//Show Mouse Icon if Moving
	if(CharacterMove) {
		drawimage(offsx + 2, offsy + 2, 20);
	}
	//Show respawn timer if dead
	if(GameState == GAMESTATE_DEAD) {
		//gray out portrait
		chcolor(COLOR_UNUSABLE, 1);
		fillrect(offsx + 2, offsy + 2, 96, 96);
		//Show respawn timer
		chcolor(COLOR_TEXTERROR, 1);
		drawstringf(offsx + 2, offsy + 2, "%d", StateChangeTimer);
	}
	//Draw hp bar
	double chp = 0;
	double fhp = 1;
	if(is_range(PlayingCharacterID, 0, MAX_OBJECT_COUNT - 1) ) {
		LookupResult_t t2;
		lookup(Gobjs[PlayingCharacterID].tid, &t2);
		chp = Gobjs[PlayingCharacterID].hp;
		fhp = t2.inithp;
		chcolor(COLOR_TEXTCMD, 1);
		//Draw current speed
		drawstringf(offsx + 120, offsy + 75, "X: %.1f Y: %.1f", Gobjs[PlayingCharacterID].sx, Gobjs[PlayingCharacterID].sy);
	}
	draw_hpbar(offsx + 100, offsy + 53, LHOTBAR_WIDTH - 100, 10, chp, fhp, COLOR_ENEMY, COLOR_ALLY);
	//Show Earth HP If there's earth
	if(is_range(EarthID, 0, MAX_OBJECT_COUNT - 1) && Gobjs[EarthID].tid == TID_EARTH) {
		//Show Earth Icon (Earth HP)
		drawimage(offsx + 100, offsy + 67, IMG_EARTH_ICO);
		//Draw Earth HP bar
		LookupResult_t t;
		lookup(TID_EARTH, &t);
		draw_hpbar(offsx + 117, offsy + 67, LHOTBAR_WIDTH - 122, 10, Gobjs[EarthID].hp, t.inithp, COLOR_ENEMY, COLOR_ALLY);
	}
	//show skill images and cooldown (if needed)
	char* L[] = {"Q", "W", "E"};
	for(uint8_t i = 0; i < SKILL_COUNT; i++) {
		double tx = offsx + 102 + (i * 54);
		if(is_range(t.skillimageids[i], 0, IMAGE_COUNT - 1) ) {
			drawimage_scale(tx, offsy + 2, 48, 48, t.skillimageids[i]);
		}
		chcolor(COLOR_TEXTCMD, 1);
		hollowrect(tx, offsy + 2, 48, 48);
		//If it is in cooldown, grayout button in LOL way
		if(SkillCooldownTimers[i] != 0) {
			//LOL like gray out
			uint16_t r = (uint16_t)scale_number(SkillCooldownTimers[i], t.skillcooldowns[i], 360);
			double p[12] = {0, -24, 24, 0, 0, 48, -48, 0, 0, -48, 24, 0};
			uint8_t np = 6;
			if(is_range(r, 0, 45)) {
				//half of top line of rect (x: 0 - 24)
				p[2] = scale_number(r, 45, 24); //x line grow left
				np = 2;
			} else if(is_range(r, 46, 135) ) {
				//left line of rect (y: -24 - 24)
				p[5] = scale_number(r - 46, 89, 48); //y line grow down
				np = 3;
			} else if(is_range(r, 136, 225) ) {
				//bottom line of rect (x: 24 - -24)
				p[6] = -scale_number(r - 136, 89, 48); //x line grow right
				np = 4;
			} else if(is_range(r, 226, 315) ) {
				//right line of rect (y: 24 - -24)
				p[9] = -scale_number(r - 226, 89, 48); //y line grow up
				np = 5;
			} else {
				//half of top lineof rect (x -24 - 0)
				p[10] = scale_number(r - 316, 44, 24); //x line grows right
				np = 6;
			}
			chcolor(COLOR_TEXTBG, 1);
			draw_polygon(tx + 24, offsy + 2 + 24, np, p);
		}
		if(SkillCooldownTimers[i] == 0) {
			//Draw QWE Label
			chcolor(COLOR_TEXTCMD, 1);
			drawstring(tx + 2, offsy + 2 + 24, L[i]);
		} else {
			//Draw timer
			chcolor(COLOR_TEXTERROR, 1);
			drawstringf(tx + 2, offsy + 2 + 24, "%d", SkillCooldownTimers[i]);
		}
	}
}

void draw_mchotbar(double offsx, double offsy) {
	//Show item candidates (Minecraft hotbar style)
	for(uint8_t i = 0; i < ITEM_COUNT; i++) {
		double tx = offsx + (i * 50);
		if(SelectingItemID == i) {
			//When selected, show description in bottom
			chcolor(COLOR_TEXTCMD, 1);
			drawstring_inwidth(offsx, offsy + 52, (char*)getlocalizeditemdesc(SelectingItemID), WINDOW_WIDTH / 2, COLOR_TEXTBG);
			//When selected, change color
			chcolor(COLOR_TEXTCHAT, 1);
		} else {
			//When unselected
			chcolor(COLOR_TEXTBG, 1);
		}
		fillrect(tx, offsy, 48, 48);
		//Show Item Image (hotbar)
		drawimage_scale(tx, offsy, 48, 48, ITEMIMGIDS[i]);
		//calculate position of additional text
		double ty = offsy + 24; //Center of width
		//If in cooldown, show cooldowntimer orelse show item value
		if(ItemCooldownTimers[i] != 0) {
			chcolor(COLOR_TEXTERROR, 1);
			drawstringf(tx, ty, "%d", ItemCooldownTimers[i]);
		} else {
			chcolor(COLOR_TEXTCMD, 1);
			drawstringf(tx, ty, "%d", ITEMPRICES[i]);
		}
		//draw unusable icon if unavailable
		if(ItemCooldownTimers[i] != 0 || Money < ITEMPRICES[i]) {
			drawimage(tx, offsy, IMG_ITEM_UNUSABLE);
		}
	}
}
