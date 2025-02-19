/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

network.h: server client common definitions (like commands)
*/

#define PACKED __attribute__((packed))
#define MAX_CLIENTS 20 //maximum count of clients
#define UNAME_SIZE 20 //max username length (includes zero)
#define PASSWD_SIZE 16 //max password length (includes zero)
#define BAN_REASON_SIZE 128 //mak ban reason length (includes zero)
#define SALT_LENGTH 16 //Server salt length
#define NET_CHAT_LIMIT 512 //Network chat length limit (includes zero)
#define SHA512_LENGTH 64 //SHA512 is 512 bits long (64 bytes)

//Server command definitions
typedef enum {
	NP_EXCHANGE_EVENTS = 0,
	NP_LOGIN_WITH_HASH = 1,
	NP_RESP_DISCONNECT = 2,
	NP_GREETINGS = 3,
	NP_RESP_LOGON = 4,
} server_command_t;

//EventType
typedef enum {
	EV_PLACE_ITEM = 0,
	EV_USE_SKILL = 1,
	EV_CHAT = 2,
	EV_RESET = 3,
	EV_HELLO = 4,
	EV_BYE = 5,
	EV_CHANGE_PLAYABLE_ID = 6,
	EV_CHANGE_ATKGAIN = 7,
	EV_PLAYABLE_LOCATION = 8
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

//Event packet
//Player coordinate broadcast
typedef struct {
	uint8_t evtype; //EV_PLAYABLE_LOCATION
	uint16_t x; //X
	uint16_t y; //Y
} PACKED ev_playablelocation_t;

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

//Chat event
typedef struct {
	uint8_t evtype; //EV_CHAT
	int8_t dstcid; //Destination cid (-1 if public message)
} PACKED ev_chat_t;

//Round reset request event (usually issued from server)
typedef struct {
	uint8_t evtype; //EV_RESET
	uint32_t level_seed; //Seed for rand()
	unsigned int basecount0 : 2; //enemy base counts
	unsigned int basecount1 : 2;
	unsigned int basecount2 : 2;
	unsigned int basecount3 : 2;
	uint16_t basedistance; //how close each enemy bases in one cluster
	float atkgain; //attack damage gain of playable
	uint8_t spawnlimit; //Spawn limit, playables can not respawn more than it.
} PACKED ev_reset_t;

//Remote user playable character id change notice event
typedef struct {
	uint8_t evtype; //EV_CHANGE_PLAYABLE_ID
	uint8_t pid; //playable character id
} PACKED ev_changeplayableid_t;

//Change atkgain event packet
typedef struct {
	uint8_t evtype; //EV_CHANGE_ATKGAIN
	float atkgain;
} PACKED ev_changeatkgain_t;
