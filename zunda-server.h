/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

zunda-server.h: server client common definitions (like commands)
*/
#define PACKED __attribute__((packed))
#define MAX_CLIENTS 20 //maximum count of clients
#define UNAME_SIZE 20 //max username length (includes zero)
#define PASSWD_SIZE 16 //max password length (includes zero)
#define SALT_LENGTH 16 //Server salt length
#define NET_CHAT_LIMIT 512 //Network chat length limit (includes zero)
#define SHA512_LENGTH 64 //SHA512 is 512 bits long (64 bytes)

//Server command definitions
typedef enum {
	NP_EXCHANGE_EVENTS = 0,
	NP_LOGIN_WITH_PASSWORD = 1,
	NP_RESP_DISCONNECT = 2,
	NP_GREETINGS = 3
} server_command_t;

//EventType
typedef enum {
	EV_CHANGE_PLAYABLE_SPEED = 0,
	EV_PLACE_ITEM = 1,
	EV_USE_SKILL = 2,
	EV_CHAT = 3,
	EV_RESET = 4,
	EV_HELLO = 5,
	EV_BYE = 6,
	EV_CHANGE_PLAYABLE_ID = 7
} event_type_t;

//Event header
typedef struct {
	int8_t cid;
	uint16_t evlen;
} PACKED event_hdr_t;

//Greeter packet
typedef struct {
	uint8_t pkttype; //NP_GREETINGS
	uint8_t cid; //client id
	uint8_t salt[SALT_LENGTH]; //salt
} PACKED np_greeter_t;

//Userlist packet header
typedef struct {
	uint8_t cid;
	uint8_t uname_len;
} PACKED userlist_hdr_t;

//Event packet
//Place item event
typedef struct {
	uint8_t evtype; //EV_PLACE_ITEM
	uint8_t tid; //ItemID
	uint16_t x; //X pos
	uint16_t y; //Y pos
} PACKED ev_placeitem_t;

//Skill use event
typedef struct {
	uint8_t evtype; //EV_USE_SKILL
	uint8_t skillid; //SkillID
} PACKED ev_useskill_t;

//Player move event
typedef struct {
	uint8_t evtype; //EV_CHANGE_PLAYABLE_SPEED
	float sx; //SpeedX
	float sy; //SppedY
} PACKED ev_changeplayablespeed_t;

//Chat event
typedef struct {
	uint8_t evtype; //EV_CHAT
	uint16_t clen; //Chat length (does not include null char)
} PACKED ev_chat_t;

//Round reset request event (usually issued from server)
typedef struct {
	uint8_t evtype; //EV_RESET
	uint32_t level_seed; //Seed for rand()
	uint32_t level_difficulty; //Round difficulty
} PACKED ev_reset_t;

//Remote user connect event (usually issued from server)
typedef struct {
	uint8_t evtype; //EV_HELLO
	uint8_t cid; //client id
	uint8_t uname_len; //login username length
} PACKED ev_hello_t;

//Remote user disconnect event (usually issued from server)
typedef struct {
	uint8_t evtype; //EV_BYE
	uint8_t cid; //client id
} PACKED ev_bye_t;

//Remote user playable character id change notice event
typedef struct {
	uint8_t evtype; //EV_CHANGE_PLAYABLE_ID
	uint8_t pid; //playable character id
} PACKED ev_changeplayableid_t;
