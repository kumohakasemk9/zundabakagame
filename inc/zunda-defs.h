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

//Version rule 1.2.3-releasedate (1 will increase if existing function name/param changed or deleted or variable/const renamed or changed, 2 will increase function added (feature add), 3 will increase if function update (bugfix)
#define CREDIT_STRING "Zundamon bakage (C) 2024 Kumohakase https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0, Zundamon is from https://zunko.jp/ (C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr"
#define CONSOLE_CREDIT_STRING "Zundamon bakage (C) 2024 Kumohakase https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0\n" \
							  "Zundamon is from https://zunko.jp/ (C) 2024 ＳＳＳ合同会社\n" \
							  "(C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr\n" \
							  "Please consider supporting me through ko-fi or pateron\n" \
							  "https://ko-fi.com/kumohakase\n" \
							  "https://www.patreon.com/kumohakasemk8\n"

#define MAX_ATKGAIN 5.0 //Maxmium AtkGain
#define MIN_ATKGAIN 0.5 //Minimum AtkGain
#define MIN_EBDIST 100 //Minimum ebdist
#define MAX_EBDIST 500 //Maximum ebdist
#define WINDOW_WIDTH 800 //Game width
#define WINDOW_HEIGHT 600 //Game height
#define MAP_WIDTH 5000 //map max width
#define MAP_HEIGHT 5000 //map max height
#define IMAGE_COUNT 35 //Preload image count
#define MAX_OBJECT_COUNT 1000 //Max object count
#define MAX_ZINDEX 3 //Max z-index
#define COLOR_TEXTBG 0x60000000 //Text background color (30% opaque black)
#define COLOR_TEXTCMD 0xff00ff00 //Command and Chat text color (Green)
#define COLOR_TEXTCHAT 0xffffffff //Chat text color
#define COLOR_TEXTERROR 0xffff0000 //Error text color: RED
#define COLOR_ENEMY 0xffff0000 //Enemy HP bar and marker color
#define COLOR_ALLY 0xff00ffa0 //Enemy HP bar and marker color
#define COLOR_UNUSABLE 0x60000000 //Gray out color
#define COLOR_KUMO9_X24_PCANNON 0xc0ffffff //kumo9 x24 pcannon color
#define MAX_CHAT_COUNT 5 //Maximum chat show count
#define BUFFER_SIZE 1024 //Command, message buffer size
#define NET_BUFFER_SIZE 8192 //Network buffer size for receiving
#define CHAT_TIMEOUT 1000 //Chat message timeout
#define ERROR_SHOW_TIMEOUT 500 //Error message timeout
#define FONT_DEFAULT_SIZE 14 //Default fontsize
#define ITEM_COUNT 5 //Max item id
#define MAX_STRINGS 26 // Max string count
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
#define TEXT_SMP_TIMEOUT 24 //Timed out
#define TEXT_OFFLINE 25 //Offline

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
	SPK_ENTER = 1,
	SPK_ESC = 2,
	SPK_BS = 3,
	SPK_LEFT = 4,
	SPK_RIGHT = 5,
	SPK_F3 = 6
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
