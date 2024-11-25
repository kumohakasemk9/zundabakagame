/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by Gtk4

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

zunda-server.h: server client common definitions (like commands)
*/
#define PACKED __attribute__((packed))
#define UNAME_SIZE 20 //max username length (includes zero)
#define PASSWD_SIZE 16 //max password length (includes zero)

//Server command definitions
typedef enum {
	NP_EXCHANGE_EVENTS = 'e',
	NP_LOGIN_WITH_PASSWORD = 'p',
	NP_RESP_DISCONNECT = 'q',
	NP_GREETINGS = 'w'
} server_command_t;

//EventType
typedef enum {
	EV_CHANGE_PLAYABLE_SPEED = 'm',
	EV_PLACE_ITEM = 'a',
	EV_USE_SKILL = 's',
	EV_CHAT = 'c',
	EV_RESET = 'r'
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
	uint32_t salt; //salt
} PACKED np_greeter_t;

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

//Round reset request event
typedef struct {
	uint8_t evtype; //EV_RESET	
} PACKED ev_reset_t;
