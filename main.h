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

//Version rule 1.2.3-releasedate (1 will increase if existing function name/param changed or deleted or variable/const renamed or changed, 2 will increase function added (feature add), 3 will increase if function update (bugfix)
#define VERSION_STRING "8.0.0-25nov2024"
#define CREDIT_STRING "Zundamon bakage (C) 2024 Kumohakase https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0, Zundamon is from https://zunko.jp/ (C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr"

#define MAX_DIFFICULTY 5 //Max difficulty
#define WINDOW_WIDTH 800 //Game width
#define WINDOW_HEIGHT 600 //Game height
#define MAP_WIDTH 5000 //map max width
#define MAP_HEIGHT 5000 //map max height
#define IMAGE_COUNT 35 //Preload image count
#define MAX_OBJECT_COUNT 1000 //Max object count
#define MAX_ZINDEX 3 //Max z-index
#define COLOR_TEXTBG 0x60ffffff //Text background color (30% opaque white)
#define COLOR_TEXTCMD 0xff00ff00 //Command and Chat text color (Green)
#define COLOR_TEXTCHAT 0xffffffff //Chat text color
#define COLOR_TEXTERROR 0xffff0000 //Error text color: RED
#define COLOR_ENEMY 0xffff0000 //Enemy HP bar and marker color
#define COLOR_ALLY 0xff00ffa0 //Enemy HP bar and marker color
#define COLOR_UNUSABLE 0x60000000 //Gray out color
#define COLOR_KUMO9_X24_PCANNON 0xc0ffffff //kumo9 x24 pcannon color
#define MAX_CHAT_COUNT 5 //Maximum chat show count
#define BUFFER_SIZE 1024 //Command, message buffer size
#define NET_RX_BUFFER_SIZE 8192 //Network buffer size for receiving
#define NET_TX_BUFFER_SIZE 8192 //Network buffer size for transmitting
#define SMP_EVENT_BUFFER_SIZE 8192 //SMP Event buffer size
#define CHAT_TIMEOUT 1000 //Chat message timeout
#define ERROR_SHOW_TIMEOUT 500 //Error message timeout
#define FONT_DEFAULT_SIZE 14 //Default fontsize
#define ITEM_COUNT 5 //Max item id
#define MAX_STRINGS 24 // Max string count
#define MAX_TID 24 //max type id
#define SKILL_COUNT 3 //Skill Count
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
#define IHOTBAR_YOFF (WINDOW_HEIGHT - 100) //Item hotbar Y offset
#define STATUS_XOFF (IHOTBAR_XOFF + (ITEM_COUNT * 50) + 10)
#define LHOTBAR_WIDTH ( ( SKILL_COUNT * 54) + 100)
#define LHOTBAR_XOFF (WINDOW_WIDTH - LHOTBAR_WIDTH) //LOL hotbar X
#define LHOTBAR_YOFF (WINDOW_HEIGHT - 100) //LOL hotbar Y
#define OBJID_INVALID -1 //Special object number, means pointing nothing
#define CHARACTER_TIMER_COUNT 4
#define KUMO9_X24_MISSILE_RANGE 500
#define KUMO9_X24_LASER_RANGE 600
#define KUMO9_X24_PCANNON_RANGE 1000
#define HOSTNAME_SIZE 64 //SMPServerInfo_t host record max len
#define PORTNAME_SIZE 6 //SMPServerInfo_t port record max len

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

//Some localized string IDs
#define TEXT_DISCONNECTED 19 //Disconnected from server
#define TEXT_BAD_COMMAND_PARAM 1 //Bad command parameter
#define TEXT_UNAVAILABLE 10 //Command or item unavailable
#define TEXT_SMP_ERROR 21 //Disconnected because error in SMP routine

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "zunda-server.h"

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
	TID_ZUNDAMON_KAMIKAZE = 16,
	TID_KUMO9_X24_MISSILE = 17,
	TID_KUMO9_X24_MISSILE_RESIDUE = 18,
	TID_MONEY_GENE = 19,
	TID_RESEARCHMENT_CENTRE = 20,
	TID_POWERPLANT = 21,
	TID_KUMO9_X24_LASER = 22,
	TID_KUMO9_X24_PCANNON = 23
} obj_type_t;

//Network status
typedef enum {
	NETWORK_DISCONNECTED = 0,
	NETWORK_CONNECTING = 1,
	NETWORK_CONNECTED = 2,
	NETWORK_LOGGEDIN = 3
} smpstatus_t;

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
	int32_t imgid; //Image id, -1 means no image
	obj_type_t tid; //Type Id, -1 means unused slot
	double x; //object location
	double y;
	double hp; //Object life
	double sx; //object speed
	double sy;
	int32_t aiming_target; //holding stalking object id
	int32_t timers[CHARACTER_TIMER_COUNT];
	int32_t hitdiameter; //when objects are within sum of both hit diameters, hitdetection will trigger
	int32_t parentid; //source id
	int32_t timeout; //lifespan, automatically decreased, dies when 0
	double damage; //given damage when hit
	int32_t srcid; //Object source id, holds who made this attack.
} GameObjs_t;

//SMP Information
typedef struct {
	char host[HOSTNAME_SIZE]; //Hostname
	char port[PORTNAME_SIZE]; //Port Number
	char usr[UNAME_SIZE]; //Username
	char pwd[PASSWD_SIZE]; //Password
} SMPProfile_t;

//constant information of characters
typedef struct {
	int32_t initimgid; //initial image id
	int32_t damage; //how opponent hp devreases when hit
	int32_t inithp; //init hp
	teamid_t teamid; //team id
	double maxspeed; //object maximum follow speed
	int32_t zindex; //object z index
	facility_type_t objecttype; //object type
	int32_t inithitdiameter; //initial hit radius
	int32_t timeout; //object lifespan
	int32_t requirepower; //requiredpower
} LookupResult_t;

//constant information of players
typedef struct {
	int32_t *skillimageids;
	int32_t *skillcooldowns;
	int32_t portraitimgid;
	int32_t portraitimg_dead_id;
	obj_type_t associatedtid;
	int32_t *skillinittimers;
	int32_t *skillranges;
} PlayableInfo_t;

//Remote player information
typedef struct {
	int32_t cid; //Client Id
	char usr[UNAME_SIZE]; //Client username
	int32_t pid; //Playable character id
	int32_t playable_objid; //Client side object id of playable character
	int32_t respawn_timer; //Client playable character respawn timer, -1 if don't need to respawn.
} SMPPlayers_t;

//main.c
void activate(GtkApplication*);
void darea_paint(GtkDrawingArea*, cairo_t*, int, int, gpointer);
void draw_game_main();
void draw_ui();
void draw_info();
void read_creds();
gboolean keypress_handler(GtkWidget*, guint, guint, GdkModifierType, gpointer);
void keyrelease_handler(GtkWidget*, guint, guint, GdkModifierType, gpointer);
void mousemotion_handler(GtkWidget*, gdouble, gdouble, gpointer);
gboolean gameinit();
gboolean drawtimer_tick(gpointer);
void draw_game_object(int32_t, LookupResult_t, double, double);
void draw_shapes(int32_t, LookupResult_t, double, double);
gboolean mousescroll_handler(GtkWidget*, gdouble, gdouble, gpointer);
void mousepressed_handler(GtkWidget*, gint, gdouble, gdouble, gpointer);
void draw_hpbar(double, double, double, double, double, double, uint32_t, uint32_t);
void draw_mchotbar(double, double);
void draw_lolhotbar(double, double);

//graphics.c
void drawline(double, double, double, double, double);
void fillcircle(double, double, double);
void drawstring(double, double, char*);
void set_font_size(int32_t);
void loadfont(const char*);
void drawstringf(double, double, const char*, ...);
void drawsubstring(double, double, char*, int32_t, int32_t);
double drawstring_inwidth(double, double, char*, int32_t, gboolean);
void drawimage(double, double, int32_t);
void drawimage_scale(double, double, double, double, int32_t);
void fillrect(double, double, double, double);
void hollowrect(double, double, double, double);
void chcolor(uint32_t, gboolean);
void restore_color();
int32_t drawstring_title(double, char*, int32_t);
void draw_polygon(double, double, int32_t, double[]);

//util.c
double scale_number(double, double, double);
double constrain_number(double, double, double);
uint32_t constrain_ui32(uint32_t, uint32_t, uint32_t);
int32_t constrain_i32(int32_t, int32_t, int32_t);
void utf8_substring(char*, int32_t, int32_t, char*, int32_t);
gboolean utf8_insertstring(char*, char*, int32_t, gsize);
void die(const char*, ...);
int32_t get_font_height();
int32_t get_string_width(char*);
int32_t shrink_string(char*, int32_t, int32_t*);
int32_t get_substring_width(char*, int32_t, int32_t);
int32_t shrink_substring(char*, int32_t, int32_t, int32_t, int32_t*);
gboolean is_range(int32_t, int32_t, int32_t);
gboolean is_range_number(double, double, double);
int32_t randint(int32_t, int32_t);
void get_image_size(int32_t, double*, double*);

//gamesys.c
void use_skill(int32_t, int32_t, PlayableInfo_t);
void local2map(double, double, double*, double*);
void getlocalcoord(int32_t, double*, double*);
void chat(char*);
void chatf(const char*, ...);
int32_t add_character(obj_type_t, double, double, int32_t);
double get_distance(int32_t, int32_t);
void set_speed_for_following(int32_t);
void set_speed_for_going_location(int32_t, double, double, double);
int32_t find_nearest_unit(int32_t, int32_t, facility_type_t);
int32_t find_random_unit(int32_t, int32_t, facility_type_t);
gboolean gametick(gpointer);
void start_command_mode(gboolean);
void switch_character_move();
void execcmd();
void use_item();
void proc_playable_op();
gboolean buy_facility(int32_t fid);
void debug_add_character();
void clipboard_read_handler(GObject*, GAsyncResult*, gpointer);
void commandmode_keyhandler(guint, GdkModifierType);
void reset_game();
void aim_earth(int32_t);
double get_distance_raw(double, double, double, double);
void select_next_item();
void select_prev_item();
void spawn_playable_me();
int32_t spawn_playable(int32_t);
void chat_request(char*);
void chatf_request(const char*, ...);

//info.c
void lookup(obj_type_t, LookupResult_t*);
void check_data();
const char *getlocalizedstring(int32_t);
const char *getlocalizeditemdesc(int32_t);
const char *getlocalizedcharactername(int32_t);
void lookup_playable(int32_t, PlayableInfo_t*);
int32_t is_playable_character();
int32_t find_playable_id_from_tid(int32_t);

//smp.h
void process_smp();
void process_smp_events(uint8_t*, size_t, int32_t);
void stack_packet(event_type_t, ...);
int32_t lookup_smp_player_from_cid(int32_t);

//aiproc.c
void procai();
gboolean procobjhit(int32_t, int32_t, LookupResult_t, LookupResult_t);
void damage_object(int32_t, int32_t);

//network.c
void connect_server();
void close_connection(int32_t);
void net_recv_handler();
void net_server_send_cmd(server_command_t);
void close_connection_silent();

//osdep.c
int32_t make_tcp_socket(char*, char*);
int32_t close_tcp_socket();
int32_t send_tcp_socket(uint8_t*, size_t);
int32_t install_io_handler();
ssize_t recv_tcp_socket(uint8_t*, size_t);
