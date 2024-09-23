/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by Gtk4

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

main.h: integrated header file
*/

//Version rule 1.2.3-releasedate (1 will increase if existing function name/param changed or deleted or variable/const renamed or changed, 2 will increase function updated or added (feature add), 3 will increase if function update (bugfix)
#define VERSION_STRING "2.1.2-sep192024"
#define CREDIT_STRING "Zundamon bakage (C) 2024 Kumohakase https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0, Zundamon is from https://zunko.jp/ (C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr"

#define WINDOW_WIDTH 800 //Game width
#define WINDOW_HEIGHT 600 //Game height
#define MAP_WIDTH 5000 //map max width
#define MAP_HEIGHT 5000 //map max height
#define IMAGE_COUNT 24 //Preload image count
#define MAX_OBJECT_COUNT 1000 //Max object count
#define MAX_ZINDEX 3 //Max z-index
#define COLOR_TEXTBG 0x60ffffff //Text background color (30% opaque white)
#define COLOR_TEXTCMD 0xff00ff00 //Command and Chat text color (Green)
#define COLOR_TEXTCHAT 0xffffffff //Chat text color
#define COLOR_TEXTERROR 0xffff0000 //Error text color: RED
#define COLOR_ENEMY 0xffff0000 //Enemy HP bar and marker color
#define COLOR_ALLY 0xff00ffa0 //Enemy HP bar and marker color
#define COLOR_UNUSABLE 0x60000000 //Gray out color
#define MAX_CHAT_COUNT 5 //Maximum chat show count
#define BUFFER_SIZE 1024 //Command, message buffer size
#define CHAT_TIMEOUT 1000 //Chat message timeout
#define ERROR_SHOW_TIMEOUT 500 //Error message timeout
#define FONT_DEFAULT_SIZE 14 //Default fontsize
#define ITEM_COUNT 3 //Max item id
#define MAX_STRINGS 12 // Max string count
#define MAX_TID 17 //max type id
#define SKILL_COUNT 3 //Skill Count
#define IMGID_ICOUNUSABLE 13 //Unusable icon id
#define PLAYABLE_CHARACTERS_COUNT 1 //Playable characters count
#define EARTH_RADAR_DIAM 500 //Earth radar diameter
#define ENEMYBASE_RADAR_DIAM 600 //Enemy base radar diameter
#define PIT_RADAR_DIAM 600 //Pit radar diameter
#define FORT_RADAR_DIAM 1000 //Fort radar diameter
#define ZUNDAMON2_RADAR_DIAM 600 //ZUNDAMON2 radar diameter
#define ZUNDAMON3_RADAR_DIAM 800 //ZUNDAMON3 radar diameter
#define PLAYABLE_AUTOMACHINEGUN_DIAM 500
#define DISTANCE_INFINITY 65535 //Infinity finddist value
#define IHOTBAR_XOFF 5 //Item hotbar X offset
#define IHOTBAR_YOFF WINDOW_HEIGHT - 100 //Item hotbar Y offset
#define STATUS_XOFF IHOTBAR_XOFF + (ITEM_COUNT * 50) + 10
#define OBJID_INVALID -1

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

//Language ID
typedef enum {
	LANGID_EN = 0,
	LANGID_JP = 1
} langid_t;

//facility type id
typedef enum {
	UNITTYPE_FACILITY = 0x1,
	UNITTYPE_UNIT = 0x2,
	UNITTYPE_BULLET = 0x4,
	UNITTYPE_BULLET_INTERCEPTABLE = 0x8
} facility_type_t;

//object type id
typedef enum {
	TID_NULL = -1,
	TID_EARTH = 0,
	TID_ENEMYBASE = 1,
	TID_ZUNDAMON1 = 2,
	TID_ZUNDAMON2 = 3,
	TID_ZUNDAMON3 = 4,
	TID_ZUNDAMONMINE = 5,
	TID_PIT = 6,
	TID_FORT = 7,
	TID_MISSILE = 8,
	TID_ALLYEXPLOSION = 9,
	TID_ENEMYEXPLOSION = 10,
	TID_EXPLOSION = 11,
	TID_ALLYBULLET = 12,
	TID_ENEMYBULLET = 13,
	TID_ENEMYZUNDALASER = 14,
	TID_KUMO9_X24_ROBOT = 15,
	TID_ZUNDAMON_KAMIKAZE = 16
} obj_type_t;

//TEAMID
typedef enum {
	TEAMID_ALLY = 0, //Ally team id
	TEAMID_ENEMY = 1, //Enemy Team Id
	TEAMID_NONE = -1 //No team id
} teamid_t;

//GameState
typedef enum {
	GAMESTATE_INITROUND = 0,
	GAMESTATE_PLAYING = 1,
	GAMESTATE_DEAD = 2,
	GAMESTATE_GAMEOVER = 3,
	GAMESTATE_GAMECLEAR = 4
} gamestate_t;

//KeyFlags
typedef enum {
	KEY_F1 = 0x1,
	KEY_F2 = 0x2,
	KEY_F3 = 0x4,
	KEY_UP = 0x10,
	KEY_DOWN = 0x20,
	KEY_LEFT = 0x40,
	KEY_RIGHT = 0x80
} keyflags_t;

//Character object
typedef struct {
	int8_t imgid; //Image id, -1 means no image
	obj_type_t tid; //Type Id, -1 means unused slot
	double x; //object location
	double y;
	double hp; //Object life
	double sx; //object speed
	double sy;
	int16_t aiming_target; //holding stalking object id
	uint16_t timer0; //automatically decreases to 0
	uint16_t timer1;
	uint16_t timer2;
	uint16_t hitdiameter; //when objects are within sum of both hit diameters, hitdetection will trigger
	int16_t parentid; //source id
	uint16_t timeout; //lifespan, automatically decreased, dies when 0
	double damage; //given damage when hit
} GameObjs_t;

//constant information of characters
typedef struct {
	int8_t initimgid; //initial image id
	uint16_t damage; //how opponent hp devreases when hit
	uint16_t inithp; //init hp
	teamid_t teamid; //team id
	double maxspeed; //object maximum follow speed
	uint8_t zindex; //object z index
	//gboolean isunkillable; //is object damageable
	//gboolean showhpbar; //can object has hp bar
	facility_type_t objecttype; //object type
	uint16_t inithitdiameter; //initial hit radius
	uint16_t timeout; //object lifespan
} LookupResult_t;

//constant information of players
typedef struct {
	int8_t *skillimageids;
	int16_t *skillcooldowns;
	int8_t portraitimgid;
	int8_t portraitimg_dead_id;
	obj_type_t associatedtid;
} PlayableInfo_t;

//main.c
void activate(GtkApplication*, gpointer);
void darea_paint(GtkDrawingArea*, cairo_t*, int, int, gpointer);
void draw_game_main();
void draw_cui();
void draw_info();
gboolean keypress_handler(GtkWidget*, guint, guint, GdkModifierType, gpointer);
void keyrelease_handler(GtkWidget*, guint, guint, GdkModifierType, gpointer);
void mousemotion_handler(GtkWidget*, gdouble, gdouble, gpointer);
gboolean gameinit();
gboolean drawtimer_tick(gpointer);
void draw_game_object(uint16_t, LookupResult_t, double, double);
void draw_shapes(uint16_t, LookupResult_t, double, double);
gboolean mousescroll_handler(GtkWidget*, gdouble, gdouble, gpointer);
void mousepressed_handler(GtkWidget*, gint, gdouble, gdouble, gpointer);
void draw_hpbar(double, double, double, double, double, double, uint32_t, uint32_t);

//graphics.c
void drawline(double, double, double, double, double);
void fillcircle(double, double, double);
//void hollowcircle(double, double, double);
void drawstring(double, double, char*);
void set_font_size(uint16_t);
void loadfont(const char*);
void drawstringf(double, double, const char*, ...);
void drawsubstring(double, double, char*, uint16_t, uint16_t);
double drawstring_inwidth(double, double, char*, uint16_t, gboolean);
void drawimage(double, double, uint8_t);
void drawimage_scale(double, double, double, double, uint8_t);
void fillrect(double, double, double, double);
void hollowrect(double, double, double, double);
void chcolor(uint32_t, gboolean);
void restore_color();
uint16_t drawstring_title(double, char*, uint8_t);
void draw_polygon(double, double, uint8_t, double[]);

//util.c
double scale_number(double, double, double);
double constrain_number(double, double, double);
uint32_t constrain_ui32(uint32_t, uint32_t, uint32_t);
int32_t constrain_i32(int32_t, int32_t, int32_t);
void utf8_substring(char*, uint16_t, uint16_t, char*, uint16_t);
gboolean utf8_insertstring(char*, char*, uint16_t, uint16_t);
void die(const char*, ...);
uint16_t get_font_height();
uint16_t get_string_width(char*);
uint16_t shrink_string(char*, uint16_t, uint16_t*);
uint16_t get_substring_width(char*, uint16_t, uint16_t);
uint16_t shrink_substring(char*, uint16_t, uint16_t, uint16_t, uint16_t*);
gboolean is_range(int32_t, int32_t, int32_t);
gboolean is_range_number(double, double, double);
int32_t randint(int32_t, int32_t);
void get_image_size(uint8_t, double*, double*);

//gamesys.c
void local2map(double, double, double*, double*);
void getlocalcoord(uint16_t, double*, double*);
void chat(char*);
void chatf(const char*, ...);
uint16_t add_character(uint8_t, double, double);
double get_distance(uint16_t, uint16_t);
void set_speed_for_following(uint16_t);
void set_speed_for_going_location(uint16_t, double, double, double);
int16_t find_nearest_unit(uint16_t, uint16_t, facility_type_t);
int16_t find_random_unit(uint16_t, uint16_t, facility_type_t);
gboolean gametick(gpointer);
void start_command_mode(gboolean);
void switch_character_move();
void execcmd();
void use_item();
void proc_playable_op();
gboolean buy_facility(uint8_t fid);
void debug_add_character();
void clipboard_read_handler(GObject*, GAsyncResult*, gpointer);
void commandmode_keyhandler(guint, GdkModifierType);
void reset_game();
void aim_earth(uint16_t);
double get_distance_raw(double, double, double, double);
void showerrorstr(uint8_t);
void select_next_item();
void select_prev_item();

//info.c
void lookup(obj_type_t, LookupResult_t*);
void check_data();
const char *getlocalizedstring(uint8_t);
const char *getlocalizeditemdesc(uint8_t);
void lookup_playable(int8_t, PlayableInfo_t*);

//aiproc.c
void procai();
gboolean procobjhit(uint16_t, uint16_t, LookupResult_t, LookupResult_t);
void damage_object(uint16_t, double, uint16_t);
