/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

zunda-defs.h: definitions
*/

//Data storage sizes
#define IMAGE_COUNT 38 //Preload image count
#define MAX_OBJECT_COUNT 1000 //Max object count
#define BUFFER_SIZE 1024 //Command, message buffer size
#define MAX_STRINGS 28 // Max string count
#define MAX_MENU_STRINGS 9 //Main menu string count
#define MAX_DEATH_STRINGS 6 //Death reason strings count
#define CHARACTER_TIMER_COUNT 4
#define MAX_TID 24 //max type id
#define PLAYABLE_CHARACTERS_COUNT 1 //Playable characters count
#define HOSTNAME_SIZE 64 //SMPServerInfo_t host record max len
#define PORTNAME_SIZE 6 //SMPServerInfo_t port record max len
#define NET_BUFFER_SIZE 8192 //Network buffer size for receiving

//Parameter limits
#define MAX_ATKGAIN 8.0 //Maxmium AtkGain
#define MIN_ATKGAIN 0.1 //Minimum AtkGain
#define MIN_EBDIST 100 //Minimum ebdist
#define MAX_EBDIST 600 //Maximum ebdist
#define MAX_EBCOUNT 4 //Maxmium ebcount
#define MAX_SPAWN_COUNT 20 //Maximum revival count
#define MAX_ZINDEX 3 //Max z-index

//Sizes
#define MAP_WIDTH 5000 //map max width
#define MAP_HEIGHT 5000 //map max height
#define WINDOW_WIDTH 800 //Game width
#define WINDOW_HEIGHT 600 //Game height
#define MAX_CHAT_COUNT 5 //Maximum chat show count
#define ITEM_COUNT 5 //Max item id
#define SKILL_COUNT 3 //Skill Count

//Game Settings
#define EARTH_RADAR_DIAM 500 //Earth radar diameter
#define ENEMYBASE_RADAR_DIAM 600 //Enemy base radar diameter
#define PIT_RADAR_DIAM 600 //Pit radar diameter
#define FORT_RADAR_DIAM 1000 //Fort radar diameter
#define ZUNDAMON2_RADAR_DIAM 600 //ZUNDAMON2 radar diameter
#define ZUNDAMON3_RADAR_DIAM 800 //ZUNDAMON3 radar diameter
#define PLAYABLE_AUTOMACHINEGUN_DIAM 500
#define KUMO9_X24_MISSILE_RANGE 500
#define KUMO9_X24_LASER_RANGE 600
#define KUMO9_X24_PCANNON_RANGE 1000
#define UNIT_STALK_LIMIT 200 //Limit distance for following (not enforced except unit)
#define FONT_DEFAULT_SIZE 14 //Default fontsize

//Some localized string IDs
#define TEXT_BAD_COMMAND_PARAM 1 //Bad command parameter

//Others
#define OBJID_INVALID -1 //Special object number, means pointing nothing
#define DISTANCE_INFINITY 65535 //Infinity finddist value

//Enums
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

//enum for passing special key pressed event
typedef enum {
	SPK_NONE = 0,
	SPK_LEFT = 1,
	SPK_RIGHT = 2,
	SPK_F3 = 3,
	SPK_UP = 4,
	SPK_DOWN = 5
} specialkey_t;

//enum for passing mouse event
typedef enum {
	MB_NONE = 0,
	MB_LEFT = 1,
	MB_RIGHT = 2,
	MB_UP = 3,
	MB_DOWN = 4
} mousebutton_t;

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
	GAMESTATE_GAMECLEAR = 4,
	GAMESTATE_TITLE = 5
} gamestate_t;

//KeyFlags
typedef enum {
	KEY_F1 = 0x1,
	KEY_F2 = 0x2,
	KEY_F3 = 0x4,
	KEY_UP = 0x10,
	KEY_DOWN = 0x20,
	KEY_LEFT = 0x40,
	KEY_RIGHT = 0x80,
	KEY_HELP = 0x100
} keyflags_t;
