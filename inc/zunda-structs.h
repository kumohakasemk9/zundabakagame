/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

zunda-structs.h: structure types
*/

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
