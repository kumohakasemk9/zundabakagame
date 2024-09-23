/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by Gtk4

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

main.c: main window process, game process (keyevents) and drawing

TODO List
add ult
Make me ahri
fix key/mouse mechanism (squueze it in gametick)
change moving system
Make it multiplay - Integrate with kumo auth system
make uint* to int*
Fix gameover/gameclear speed issue
*/

#include "main.h"

cairo_surface_t *Gsfc; //GameScreen surface
cairo_t* G; //Gamescreen cairo context
GtkWidget *GameDrawingArea; //Gamescreen drawing area
cairo_surface_t *Plimgs[IMAGE_COUNT]; //Preloaded images
GtkApplication *Application; //Application
GameObjs_t Gobjs[MAX_OBJECT_COUNT]; //Game Objects
uint16_t CameraX = 0, CameraY = 0; //Camera Position
extern const char* IMGPATHES[];
int16_t CommandCursor = -1; //Command System Related
char CommandBuffer[BUFFER_SIZE];
uint16_t ChatTimeout = 0; //Remaining time to show chat
char ChatMessages[MAX_CHAT_COUNT][BUFFER_SIZE]; //Chat message buffer
GdkClipboard* GClipBoard;
obj_type_t AddingTID;
uint16_t CursorX, CursorY; //Current Cursor Pos
gboolean DebugMode = FALSE;
gamestate_t GameState;
int16_t PlayingCharacterID = -1; //Current Playable Character ID
uint16_t EarthID; //Current Earth ID
uint32_t Money;
gboolean CharacterMove; //Character Moving on/off
int8_t SelectingItemID;
extern const uint16_t ITEMPRICES[ITEM_COUNT];
extern const uint8_t ITEMIMGIDS[ITEM_COUNT];
langid_t LangID = LANGID_JP;
int8_t RecentErrorId = -1;
uint16_t ErrorShowTimer = 0;
uint16_t StateChangeTimer;
double PlayerSpeed;
uint16_t ItemCooldownTimers[ITEM_COUNT];
GtkGesture* MouseGestureCatcher;
uint16_t SkillCooldownTimers[SKILL_COUNT];
uint16_t SkillEnableTimers[SKILL_COUNT];
int8_t CurrentPlayableCharacterID = 0;
keyflags_t KeyFlags;
gboolean ProgramExiting = FALSE;
PangoLayout *Gpangolayout = NULL;
uint8_t SkillStates[SKILL_COUNT];

//Paint event of Drawing Area, called for every 30mS
void darea_paint(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
	//Clear screen
	cairo_set_source_rgb(G, 0, 0, 0);
	cairo_paint(G);
	//Draw game
	draw_game_main();
	draw_cui();
	draw_info();
	//Apply screen
	cairo_set_source_surface(cr, Gsfc, 0, 0);
	cairo_paint(cr);
}

//Called when app started
void activate(GtkApplication* app, gpointer user_data) {
	//Prepare game screen
	GameDrawingArea = gtk_drawing_area_new();
	gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(GameDrawingArea), WINDOW_WIDTH);
	gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(GameDrawingArea), WINDOW_HEIGHT);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(GameDrawingArea), darea_paint, NULL, NULL);
	//Prepare game screen
	Gsfc = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WINDOW_WIDTH, WINDOW_HEIGHT);
	G = cairo_create(Gsfc);
	Gpangolayout = pango_cairo_create_layout(G); //Setup PangoLayout for font drawing
	//Prepare main window
	GtkWidget *w = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(w), "SampleGame");
	gtk_window_set_child(GTK_WINDOW(w), GameDrawingArea);
	gtk_window_set_resizable(GTK_WINDOW(w), FALSE);
	gtk_window_present(GTK_WINDOW(w));
	if(gameinit() == FALSE) {
		die("main.c: activate(): gameinit() failed\n");
		return;
	}
	GClipBoard = gtk_widget_get_clipboard(w);
	//Prepare timers
	g_timeout_add(10, gametick, NULL);
	g_timeout_add(30, drawtimer_tick, NULL);
	//Prepare KeyEvent catcher
	GtkEventController *c = gtk_event_controller_key_new();
	g_signal_connect_object(c, "key-pressed", G_CALLBACK(keypress_handler), GameDrawingArea, G_CONNECT_SWAPPED);
	g_signal_connect_object(c, "key-released", G_CALLBACK(keyrelease_handler), GameDrawingArea, G_CONNECT_SWAPPED);
	gtk_widget_add_controller(GTK_WIDGET(w), c);
	//Prepare mousemotion catcher
	GtkEventController *c2 = gtk_event_controller_motion_new();
	g_signal_connect_object(c2, "motion", G_CALLBACK(mousemotion_handler), GameDrawingArea, G_CONNECT_SWAPPED);
	gtk_widget_add_controller(GTK_WIDGET(w), c2);
	//Prepare scroll Handler
	GtkEventController *c3 = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
	g_signal_connect_object(c3, "scroll", G_CALLBACK(mousescroll_handler), GameDrawingArea, G_CONNECT_SWAPPED);
	gtk_widget_add_controller(GTK_WIDGET(w), c3);
	//Prepare right clicking handler
	MouseGestureCatcher = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(MouseGestureCatcher), 0);
	g_signal_connect_object(MouseGestureCatcher, "pressed", G_CALLBACK(mousepressed_handler), GameDrawingArea, G_CONNECT_SWAPPED);
	gtk_widget_add_controller(GTK_WIDGET(w), GTK_EVENT_CONTROLLER(MouseGestureCatcher) );
}

gboolean drawtimer_tick(gpointer data) {
	if(!ProgramExiting) {
		gtk_widget_queue_draw(GameDrawingArea);
		return TRUE;
	} else {
		return FALSE;
	}
}

void mousepressed_handler(GtkWidget* wid, gint n_press, gdouble x, gdouble y, gpointer user_data) {
	guint i = gtk_gesture_single_get_current_button( GTK_GESTURE_SINGLE(MouseGestureCatcher) );
	if(i == 1) {
		switch_character_move();
	} else if(i == 3) {
		use_item();
	}
}

gboolean mousescroll_handler(GtkWidget* wid, gdouble dx, gdouble dy, gpointer user_data) {
	if(dy < 0) {
		select_next_item();
	} else {
		select_prev_item();
	}
	return TRUE;
}

int main(int argc, char *argv[]) {
	int s;
	Application = gtk_application_new("kumotechmadlab.kumohakase.zundamonbakage", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(Application, "activate", G_CALLBACK(activate), NULL);
	s = g_application_run(G_APPLICATION(Application), argc, argv);
	//Finalize
	ProgramExiting = TRUE;
	for(uint8_t i = 0; i < IMAGE_COUNT; i++) {
		if(Plimgs[i] != NULL) {
			cairo_surface_destroy(Plimgs[i]);
		}
	}
	//Those variables will created before possible call event of die();
	//and no NULL check needed because they must be initialized.
	g_object_unref(Application);
	g_object_unref(Gpangolayout);
	cairo_destroy(G);
	cairo_surface_destroy(Gsfc);
	return s;
}

void draw_game_main() {
	//draw characters
	for(uint8_t z = 0; z < MAX_ZINDEX; z++) {
		for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
			if(Gobjs[i].tid == TID_NULL) {continue;} //Skip if slot is not used.
			LookupResult_t t;
			lookup((uint8_t)Gobjs[i].tid, &t);
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
						double px = constrain_number(x, 2, WINDOW_WIDTH - 18);
						double py = constrain_number(y, 2, WINDOW_HEIGHT - 110 - 16);
						int8_t iid = -1;
						switch(Gobjs[i].tid) {
						case TID_EARTH: iid = 17;
						break;
						case TID_PIT: iid = 21;
						break;
						case TID_FORT:
							iid = 23;
						break;
						case TID_ENEMYBASE: iid = 9;
						break;
						default:
						break;
						}
						if(iid != -1) { drawimage(px, py, (uint8_t)iid); }
						/*if(t.teamid == TEAMID_ALLY) {
							chcolor(COLOR_ALLY, TRUE);
							fillcircle(px, py, 10);
						} else {
							chcolor(COLOR_ENEMY, TRUE);
							fillrect(px - 5, py - 5, 10, 10);
						}*/
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

void draw_game_object(uint16_t idx, LookupResult_t t, double x, double y) {
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
	get_image_size((uint8_t)Gobjs[idx].imgid, &w, &h);
	x = x - (w / 2.0);
	y = y - (h / 2.0);
	drawimage(x, y, (uint8_t)Gobjs[idx].imgid);
	if(DebugMode) {
		chcolor(0xffffffff, TRUE);
		hollowrect(x, y, w, h);
		drawstringf(x, y, "ID=%d", idx);
		chcolor(0x30ffffff, TRUE);
		fillcircle(x + (w / 2.0), y + (h / 2.0), Gobjs[idx].hitdiameter);
	}
	//Draw HP bar if their initial HP is not 0
	if(t.inithp != 0) {
		uint32_t cc = COLOR_ENEMY;
		//change fgcolor by team
		if(t.teamid == TEAMID_ALLY) {
			cc = COLOR_ALLY;
		}
		draw_hpbar(x, y - 7, w, 2, Gobjs[idx].hp, t.inithp, COLOR_TEXTCHAT, cc);
	}
}

void draw_hpbar(double x, double y, double w, double h, double chp, double fhp, uint32_t colorbg, uint32_t colorfg) {
	chcolor(colorbg, TRUE);
	fillrect(x, y, w, h);
	double bw = scale_number(chp, fhp, w);
	chcolor(colorfg, TRUE);
	fillrect(x, y, bw, h);
}

void draw_shapes(uint16_t idx, LookupResult_t t, double x, double y) {
	if(!is_range(idx, 0, MAX_OBJECT_COUNT - 1)) {
		die("draw_game_object(): bad idx, how can you do that!?\n");
		return;
	}
	//draw shapes
	double d = 0;
	if(DebugMode) {
		chcolor(0x5000ff00, TRUE);
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
			default:
			break;
		}
	}
	if(Gobjs[idx].tid == TID_ALLYEXPLOSION || Gobjs[idx].tid == TID_ENEMYEXPLOSION || Gobjs[idx].tid == TID_EXPLOSION) {
		d = Gobjs[idx].hitdiameter;
	}
	switch(Gobjs[idx].tid) {
		case TID_ALLYEXPLOSION:
			chcolor(0x7000A0FF, TRUE);
		break;
		case TID_ENEMYEXPLOSION:
			chcolor(0x70ffa000, TRUE);
		break;
		case TID_EXPLOSION:
			chcolor(0x70ff0000, TRUE);
		break;
		default:
		break;
	}
	if(d != 0) {
		//Draw circle
		fillcircle(x, y, d);
	} else if(Gobjs[idx].tid == TID_ENEMYZUNDALASER) {
		//Draw laser obj
		if(is_range(Gobjs[idx].aiming_target, 0, MAX_OBJECT_COUNT - 1)) {
			chcolor(0xa000ff00, TRUE);
			double sx = x;
			double sy = y;
			if(is_range(Gobjs[idx].parentid, 0, MAX_OBJECT_COUNT - 1) ) {
				getlocalcoord( (uint16_t)Gobjs[idx].parentid, &sx, &sy);
			}
			double tx;
			double ty;
			getlocalcoord((uint16_t)Gobjs[idx].aiming_target, &tx, &ty);
			drawline(sx, sy, tx, ty, 20);
			chcolor(0xa0ff0000, TRUE);
			fillcircle(tx, ty, 50);
		} else {
			//g_print("main.c: draw_game_main(): WARNING: ID%d is laser but it has bad target id!\n", idx);
		}
	}
}

void draw_cui() {
	//draw ingame cui
	uint16_t fh = get_font_height();
	//Show command input window if command mode
	if(CommandCursor != -1) {
		//Shrink string to fit in screen
		uint16_t sp = 0;
		uint16_t w = WINDOW_WIDTH + 10;
		uint16_t l = (uint16_t)g_utf8_strlen(CommandBuffer, 65535);
		while(w > WINDOW_WIDTH - 5) {
			w = get_substring_width(CommandBuffer, sp, (uint16_t)CommandCursor);
			if(w <= WINDOW_WIDTH - 5 || sp >= l - 1) { break; }
			sp++;
		}
		//Show command buffer
		chcolor(COLOR_TEXTBG, TRUE); //White (70% transparent)
		fillrect(0, 0, WINDOW_WIDTH, fh + 2);
		chcolor(COLOR_TEXTCMD, TRUE); //Green
		drawsubstring(0, 0, CommandBuffer, sp, 65535);
		drawline(w, 0, w ,fh + 2, 1);
	}
}

void draw_info() {
	//Draw additional information
	uint16_t fh = get_font_height();
	//Show message window if there are message
	if(CommandCursor != -1 || ChatTimeout != 0) {
		chcolor(COLOR_TEXTCHAT, TRUE);
		double ty = (double)fh + 2;
		for(uint8_t i = 0; i < MAX_CHAT_COUNT; i++) {
			if(g_utf8_strlen(ChatMessages[i], 65535) != 0) {
				//print(m)
				ty = drawstring_inwidth(0, ty, ChatMessages[i], WINDOW_WIDTH - 5, TRUE);
			}
		}
	}
	//Draw Error Message if there are error
	if(ErrorShowTimer != 0 && RecentErrorId != -1) {
		chcolor(COLOR_TEXTERROR, TRUE);
		if(!is_range(RecentErrorId, 0, MAX_STRINGS - 1) ) {
			die("draw_info(): bad RecentErrorId!\n");
			return;
		}
		drawstring_title(WINDOW_WIDTH / 2, (char*)getlocalizedstring((uint8_t)RecentErrorId), FONT_DEFAULT_SIZE);
	}
	//Draw Message when GAMESTATE != PLAYING
	if(GameState != GAMESTATE_PLAYING) {
		chcolor(COLOR_TEXTCMD, TRUE);
		uint16_t r;
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
		default:
			break;
		}
	}
	//If debug mode, show camera and cursor pos
	if(DebugMode) {
		chcolor(COLOR_TEXTCMD, TRUE);
		drawstringf(0, WINDOW_HEIGHT - 120 - fh, "(Cheat Mode) Camera=(%d,%d) Cursor=(%d,%d) KeyFlags=0x%x", CameraX, CameraY, CursorX, CursorY, KeyFlags);
	}
	//Show item candidates (Minecraft hotbar style)
	for(uint8_t i = 0; i < ITEM_COUNT; i++) {
		double tx = IHOTBAR_XOFF + (i * 50);
		if(SelectingItemID == i) {
			//When selected, change color
			chcolor(COLOR_TEXTCHAT, TRUE);
		} else {
			//When unselected
			chcolor(COLOR_TEXTBG, TRUE);
		}
		fillrect(tx, IHOTBAR_YOFF, 48, 48);
		//Show Item Image (hotbar)
		drawimage_scale(tx, IHOTBAR_YOFF, 48, 48, ITEMIMGIDS[i]);
		//calculate position of additional text
		double ty = IHOTBAR_YOFF + 24; //Center of width
		//If in cooldown, show cooldowntimer orelse show item value
		if(ItemCooldownTimers[i] != 0) {
			chcolor(COLOR_TEXTERROR, TRUE);
			drawstringf(tx, ty, "%d", ItemCooldownTimers[i]);
		}
		//draw unusable icon if unavailable
		if(ItemCooldownTimers[i] != 0 || Money < ITEMPRICES[i]) {
			drawimage(tx, IHOTBAR_YOFF, IMGID_ICOUNUSABLE);
		}
	}
	//const double ddd = 10 + (ITEM_COUNT * 50);
	//Show Money value
	//Show Money Icon
	drawimage(STATUS_XOFF, IHOTBAR_YOFF, 18);
	//Show current Money
	chcolor(COLOR_TEXTCMD, TRUE);
	drawstringf(STATUS_XOFF + 16, IHOTBAR_YOFF, "%d", Money);
	//Show Mouse Icon if Moving
	if(CharacterMove) {
		drawimage(STATUS_XOFF, IHOTBAR_YOFF + 16, 20);
	}
	//Show Item desc and value
	if(is_range(SelectingItemID, 0, ITEM_COUNT - 1) ) {
		drawstring_inwidth(IHOTBAR_XOFF, IHOTBAR_YOFF + 52, (char*)getlocalizeditemdesc((uint8_t)SelectingItemID), (uint16_t)(WINDOW_WIDTH / 2), COLOR_TEXTBG);
		drawstringf(STATUS_XOFF + 16, IHOTBAR_YOFF + 24, "Price: %d", ITEMPRICES[SelectingItemID]);
	}
	double hbw = ( SKILL_COUNT * 50) + 100; //hotbar width: skill showing area + portrait area + spaces
	double txo = WINDOW_WIDTH - hbw - 5; //centering x-off

	//Show LOL like hotbar
	PlayableInfo_t t;
	lookup_playable(0, &t);
	chcolor(COLOR_TEXTBG, TRUE);
	fillrect(txo, WINDOW_HEIGHT - 100, hbw, 100); //show bg
	//show portrait image
	drawimage(txo + 2, WINDOW_HEIGHT - 98, (uint8_t)t.portraitimgid);
	//Show respawn timer if dead
	if(GameState == GAMESTATE_DEAD) {
		//gray out portrait
		chcolor(COLOR_UNUSABLE, TRUE);
		fillrect(txo + 2, WINDOW_HEIGHT - 98, 96, 96);
		//Show respawn timer
		chcolor(COLOR_TEXTERROR, TRUE);
		drawstringf(txo + 2, WINDOW_HEIGHT - 98, "%d", StateChangeTimer);
	}
	//Draw hp bar
	double chp = 0;
	double fhp = 1;
	if(is_range(PlayingCharacterID, 0, MAX_OBJECT_COUNT - 1) ) {
		LookupResult_t t2;
		lookup((uint8_t)Gobjs[PlayingCharacterID].tid, &t2);
		chp = Gobjs[PlayingCharacterID].hp;
		fhp = t2.inithp;
	}
	draw_hpbar(txo + 100, WINDOW_HEIGHT - 45, hbw - 102, 10, chp, fhp, COLOR_ENEMY, COLOR_ALLY);
	//Show Earth HP If there's earth
	if(is_range(EarthID, 0, MAX_OBJECT_COUNT - 1) && Gobjs[EarthID].tid == TID_EARTH) {
		//Show Earth Icon (Earth HP)
		drawimage(txo + 100, WINDOW_HEIGHT - 30, 17);
		//Draw Earth HP bar
		LookupResult_t t;
		lookup(TID_EARTH, &t);
		draw_hpbar(txo + 120, WINDOW_HEIGHT - 30, hbw - 122, 10, Gobjs[EarthID].hp, t.inithp, COLOR_ENEMY, COLOR_ALLY);
	}
	//show skill images and cooldown (if needed)
	char* L[] = {"Q", "W", "E"};
	for(uint8_t i = 0; i < SKILL_COUNT; i++) {
		double tx = txo + 100 + (i * 50);
		if(t.skillimageids[i] == -1) {
			chcolor(COLOR_TEXTCHAT, TRUE);
			fillrect(tx, WINDOW_HEIGHT - 98, 48, 48);
		} else {
			drawimage(tx, WINDOW_HEIGHT - 98, (uint8_t)t.skillimageids[i]);
		}
		chcolor(COLOR_TEXTCMD, TRUE);
		drawstring(tx, WINDOW_HEIGHT - 98, L[i]);
		//If it is in cooldown, grayout button in LOL way
		if(SkillCooldownTimers[i] != 0) {
			//LOL like gray out
			uint16_t r = (uint16_t)scale_number(SkillCooldownTimers[i], t.skillcooldowns[i], 360);
			double p[12] = {0, -24};
			uint8_t np = 1;
			if(is_range(r, 0, 45)) {
				//half of top line of rect (x: 0 - 24)
				p[2] = scale_number(r, 45, 24); //x line grow left
				p[3] = 0;
				np = 2;
			} else if(is_range(r, 46, 135) ) {
				//left line of rect (y: -24 - 24)
				p[2] = 24;
				p[3] = 0;
				p[4] = 0;
				p[5] = scale_number(r - 46, 89, 48); //y line grow down
				np = 3;
			} else if(is_range(r, 136, 225) ) {
				//bottom line of rect (x: 24 - -24)
				p[2] = 24;
				p[3] = 0;
				p[4] = 0;
				p[5] = 48;
				p[6] = -scale_number(r - 136, 89, 48); //x line grow right
				p[7] = 0;
				np = 4;
			} else if(is_range(r, 226, 315) ) {
				//right line of rect (y: 24 - -24)
				p[2] = 24;
				p[3] = 0;
				p[4] = 0;
				p[5] = 48;
				p[6] = -48;
				p[7] = 0;
				p[8] = 0;
				p[9] = -scale_number(r - 226, 89, 48);
				np = 5;
			} else {
				//half of top lineof rect (x -24 - 0)
				p[2] = 24;
				p[3] = 0;
				p[4] = 0;
				p[5] = 48;
				p[6] = -48;
				p[7] = 0;
				p[8] = 0;
				p[9] = -48;
				p[10] = -scale_number(r - 316, 44, 24);
				p[11] = 0;
				np = 6;
			}
			chcolor(COLOR_TEXTBG, TRUE);
			draw_polygon(tx + 24, WINDOW_HEIGHT - 66 + 24, np, p);
			//Draw timer
			chcolor(COLOR_TEXTERROR, TRUE);
			drawstringf(tx, WINDOW_HEIGHT - 66 + fh + 2, "%d", SkillCooldownTimers[i]);
		}
	}
}

gboolean gameinit() {
	//Gameinit function, load assets and more.
	//Load images into memory.
	g_print("Loading image assets.\n");
	for(guint i = 0; i < IMAGE_COUNT; i++) {
		Plimgs[i] = NULL;
	}
	for(guint i = 0; i < IMAGE_COUNT; i++) {
		Plimgs[i] = cairo_image_surface_create_from_png(IMGPATHES[i]);
		if(cairo_surface_status(Plimgs[i]) != CAIRO_STATUS_SUCCESS) {
			g_print("main.c: gameinit(): Error loading %s\n", IMGPATHES[i]);
			return FALSE;
		}
	}
	//Initialize Chat Message Slots
	for(uint8_t i = 0; i < MAX_CHAT_COUNT; i++) {
		ChatMessages[i][0] = 0;
	}
	check_data(); //Data Check
	//load fonts
	loadfont("Ubuntu Mono,monospace");
	//Detect system locale
	PangoLanguage* lang = pango_language_get_default();
	const char* lang_c = pango_language_to_string(lang);
	if(strcmp(lang_c, "ja-jp") == 0) {
		g_print("Japanese locale detected. Changing language.\n");
		LangID = LANGID_JP;
	} else {
		g_print("Changed to English mode because of your locale setting: %s\n", lang_c);
		LangID = LANGID_EN;
	}
	//cairo_set_font_size(G, FONT_DEFAULT_SIZE);*/
	set_font_size(FONT_DEFAULT_SIZE);
	reset_game(); //reset game round
	return TRUE;
}

gboolean keypress_handler(GtkWidget* wid, guint keyval, guint keycode, GdkModifierType state, gpointer data) {
	if(CommandCursor == -1) {
		//normal mode
		//self.setreset_keyflag(evt.keyval, True)
		switch(keyval) {
		case GDK_KEY_t:
		case GDK_KEY_T:
			//T
			start_command_mode(FALSE);
			break;
		case GDK_KEY_slash:
			// slash
			start_command_mode(TRUE);
			break;
		case GDK_KEY_space:
			//space
			switch_character_move();
			break;
		case GDK_KEY_A:
		case GDK_KEY_a:
			//A
			select_prev_item();
			break;
		case GDK_KEY_S:
		case GDK_KEY_s:
			//S
			select_next_item();
			break;
		case GDK_KEY_D:
		case GDK_KEY_d:
			//D
			use_item();
			break;
		case GDK_KEY_F:
		case GDK_KEY_f:
			//F (Debug Key)
			debug_add_character();
			break;
		case GDK_KEY_Q:
		case GDK_KEY_q:
			//Q
			if( (KeyFlags & KEY_F1) == 0) { KeyFlags += KEY_F1; }
			break;
		case GDK_KEY_W:
		case GDK_KEY_w:
			//W
			if( (KeyFlags & KEY_F2) == 0) { KeyFlags += KEY_F2; }
			break;
		case GDK_KEY_E:
		case GDK_KEY_e:
			//E
			if( (KeyFlags & KEY_F3) == 0) { KeyFlags += KEY_F3; }
			break;
		case GDK_KEY_J:
		case GDK_KEY_j:
			//J
			if( (KeyFlags & KEY_LEFT) == 0) { KeyFlags += KEY_LEFT; }
			break;
		case GDK_KEY_K:
		case GDK_KEY_k:
			//K
			if( (KeyFlags & KEY_DOWN) == 0) { KeyFlags += KEY_DOWN; }
			break;
		case GDK_KEY_L:
		case GDK_KEY_l:
			//L
			if( (KeyFlags & KEY_RIGHT) == 0) { KeyFlags += KEY_RIGHT; }
			break;
		case GDK_KEY_I:
		case GDK_KEY_i:
			//I
			if( (KeyFlags & KEY_UP) == 0) { KeyFlags += KEY_UP; }
			break;
		}
	} else {
		commandmode_keyhandler(keyval, state);
	}
	return FALSE;
}

void keyrelease_handler(GtkWidget* wid, guint keyval, guint keycode, GdkModifierType state, gpointer data) {
	if(CommandCursor == -1) {
		//Operate keyflags on command mode
		switch(keyval) {
		case GDK_KEY_Q:
		case GDK_KEY_q:
			//Q
			if(KeyFlags & KEY_F1) { KeyFlags -= KEY_F1; }
			break;
		case GDK_KEY_W:
		case GDK_KEY_w:
			//W
			if(KeyFlags & KEY_F2) { KeyFlags -= KEY_F2; }
			break;
		case GDK_KEY_E:
		case GDK_KEY_e:
			//E
			if(KeyFlags & KEY_F3) { KeyFlags -= KEY_F3; }
			break;
		case GDK_KEY_J:
		case GDK_KEY_j:
			//J
			if(KeyFlags & KEY_LEFT) { KeyFlags -= KEY_LEFT; }
			break;
		case GDK_KEY_K:
		case GDK_KEY_k:
			//K
			if(KeyFlags & KEY_DOWN) { KeyFlags -= KEY_DOWN; }
			break;
		case GDK_KEY_L:
		case GDK_KEY_l:
			//L
			if(KeyFlags & KEY_RIGHT) { KeyFlags -= KEY_RIGHT; }
			break;
		case GDK_KEY_I:
		case GDK_KEY_i:
			//I
			if(KeyFlags & KEY_UP) { KeyFlags -= KEY_UP; }
			break;
		}
	}
}

void mousemotion_handler(GtkWidget *wid, gdouble x, gdouble y, gpointer user_data) {
	//Called when mouse moved, save mouse location value
	CursorX = (uint16_t)x;
	CursorY = (uint16_t)y;
}
