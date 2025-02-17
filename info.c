/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

info.c: game object information and localization info
*/

#include "inc/zundagame.h"

extern langid_t LangID;

//message strings
const char* JP_STRINGS[MAX_STRINGS] = {
	"施設同士の距離が近すぎます。",
	"不正なパラメータです",
	"戦闘配置!",
	"迫りくるずんだもんから地球を守ろう!",
	"死んでしまった!",
	"復活中です",
	"ずんだもん、地球征服。",
	"プレイありがとうございます。(作戦失敗)",
	"地球は守られた!",
	"プレイありがとうございます。おめでとう!",
	"現在使用出来ません",
	"爆発に巻き込まれた",
	"蜂の巣にされた",
	"大量の枝豆を浴びた",
	"カリカリに焼けた",
	"消滅した",
	"わけの分からない理由で死んだ",
	"サーバーに接続しています",
	"接続できませんでした",
	"サーバーから切断しました",
	"サーバーとの接続を確立しました",
	"SMPエラー",
	"がゲームに参加しました",
	"がゲームから切断しました",
	"応答なし",
	"接続していません"
};

const char *EN_STRINGS[MAX_STRINGS] = {
	"Planets are too close eachother!",
	"Insufficient parameter!",
	"Start operation!",
	"Protect the earth from offensive Zundamons!",
	"YOU DIED!",
	"Recovering, please wait.",
	"PWNED!",
	"Thank you for playing.",
	"Earth is safe now!",
	"Thanks for playing, Congratulations!",
	"Unavailable",
	"blown up",
	"turned into a bee hive",
	"took too much Edamames",
	"burnt into crisp",
	"annihilated",
	"got killed by uncomprehensible reason",
	"Connecting to server",
	"Server connection failed.",
	"Disconnected from server.",
	"Connection established.",
	"SMP error.",
	"joined in the game.",
	"left from the game.",
	"Timed out",
	"Offline"
};

//TID names LUT
const char *JP_TID_NAMES[MAX_TID] = {
	"地球",
	"ずんだもん星",
	"ざこずん",
	"おこずん",
	"ぷんぷんずん",
	"ずんだもん地雷",
	"入渠",
	"要塞",
	"見方ミサイル",
	"見方爆発",
	"敵爆発",
	"爆発",
	"見方弾",
	"敵弾",
	"敵ずんだレーザー",
	"Kumo9-x24",
	"神風もん",
	"kumo9-x24-ミサイル",
	"kumo9-x24-ミサイル爆発痕",
	"優秀な証券取引所",
	"グーグル",
	"発電所",
	"kumo9-x24-レーザー",
	"kumo9-x24-陽電子砲"
};

const char *EN_TID_NAMES[MAX_TID] = {
	"TheEarth",
	"ZundamonStar",
	"NormalZundamon",
	"MadZundamon",
	"SpicyZundamon",
	"ZundaMine",
	"Pit",
	"Fort",
	"AllyMissile",
	"AllyExplosion",
	"EnemyExplosion",
	"Explosion",
	"AllyBullet",
	"EnemyBullet",
	"EnemyZundaLaser",
	"Kumo9-x24",
	"KamikazeZunda",
	"Kumo9-x24-missile",
	"Kumo9-x24-missile-residue",
	"BestStockMarket",
	"Google",
	"PowerPlant",
	"Kumo9-x24-laser",
	"Kumo9-x24-PositronCanon"
};

//Item description strings: jp
const char* ITEMDESC_JP[ITEM_COUNT] = {
	"入渠: 周囲のユニットを回復します",
	"要塞: 周囲の敵を迎撃します",
	"優秀な証券取引所: 10秒あたり2個の部品がもらえます",
	"Google: 操作可能ユニットの速度を向上したり、地球の回復力を早めます。",
	"発電所: 電力を供給します。総発電電力は必要電力を上回っている必要があります。"
};

//Item description strings: en
const char* ITEMDESC_EN[ITEM_COUNT] = {
	"Pit: Facility that recovers nearby units.",
	"Fort: Facility that attacks nearby enemies.",
	"Best stock market: Gives 2 Parts per 10 sec.",
	"Google: Upgrades your speed and gives the earth self recovery ability.",
	"PowerPlant: Generates power, generated power must be more than required power."
};

//Hotbar Item Image ID LUT
const int32_t ITEMIMGIDS[ITEM_COUNT] = {
	14, //pit
	15, //fort
	28, //bank
	16, //techres
	30 //powerplant
};

//Item Values
const int32_t ITEMPRICES[ITEM_COUNT] = {
	10, //pit
	20, //fort
	50, //bank
	60, //techres
	20  //powerplant
};

//Facility TID vs Item ID
const obj_type_t FTIDS[ITEM_COUNT] = {
	TID_PIT, //pit
	TID_FORT, //fort
	TID_MONEY_GENE, //bank
	TID_RESEARCHMENT_CENTRE, //techres
	TID_POWERPLANT //powerplant
};

//Item cooldown defaults
const int32_t ITEMCOOLDOWNS[ITEM_COUNT] = {
	1000, //pit
	2000, //fort
	4000, //bank
	4000, //techres
	2000  //power plant
};

//Playable character skill cooldowns
int32_t SKILLCOOLDOWNS[PLAYABLE_CHARACTERS_COUNT][SKILL_COUNT] = {
	{1000, 1500, 6000} //kumo9-x24 cooldowns (kumohakasemk9)
};

//initial value of timers when skill activated
int32_t SKILL_INIT_TIMERS[PLAYABLE_CHARACTERS_COUNT][SKILL_COUNT] = {
	{500, 500, 2} //kumo9-x24
};

//skill ranges
int32_t SKILL_RANGES[PLAYABLE_CHARACTERS_COUNT][SKILL_COUNT] = {
	{KUMO9_X24_MISSILE_RANGE, KUMO9_X24_LASER_RANGE, KUMO9_X24_PCANNON_RANGE} //kumo9-x24
};

//Playable character skill icon ids LUT
int32_t SKILLICONIDS[PLAYABLE_CHARACTERS_COUNT][SKILL_COUNT] = {
	{24, 33, 34} //kumo9-x24 skill icons (kumohakasemk9)
};

//Playable character information (tid, portraitimgid, dead portrait)
const int32_t PLAYABLE_INFORMATION[PLAYABLE_CHARACTERS_COUNT][3] = {
	{15, 19, 19} //kumo9-x24 (kumohakasemk9)
};

//Image assets
const char *IMGPATHES[IMAGE_COUNT] = {
	"img/earth.png", //0 Earth
	"img/zunda_star.png", //1 EnemyBase
	"img/zunda.png", //2 Zundamon1
	"img/zunda2.png", //3 Zundamon2
	"img/zunda3.png", //4 Zundamon3
	"img/zunda_bomb.png", //5 ZundamonMine
	"img/fixing_star.png", //6 Pit
	"img/fort_star.png", //7 Fort
	"img/missile.png", //8 Missile
	"img/bullet.png", //9 ally bullet
	"img/edamame_bullet.png", //10 enemy bullet
	"img/kumo9-x24/chara.png", //11 kumo9-x24-robot (playable) (kumohakase9)
	"img/zunda_bomb_big.png", //12 Kamikaze zundamon
	"adwaitalegacy/emblem-unreadable.png", //13 Item unusable icon
	"adwaitalegacy/preferences-system.png", //14 pit hotbar icon
	"adwaitalegacy/face-angry.png", //15 fort hotbar icon
	"adwaitalegacy/preferences-other.png", //16 upgrade hotbar icon
	"adwaitalegacy/applications-internet.png", //17 earth icon (status bar)
	"adwaitalegacy/applications-system.png", //18 Money icon (status bar)
	"img/kumo9-x24/portrait.png", //19 kumo9-x24 portrait (hotbar)
	"adwaitalegacy/input-mouse.png", //20 Mouse icon (status bar)
	"adwaitalegacy/preferences-system-16.png", //21 map mark: pit
	"adwaitalegacy/image-missing.png", //22 image-missing
	"adwaitalegacy/preferences-other-16.png", //23 technology star (status)
	"img/kumo9-x24/missile.png", //24 kumo9-x24 missile
	"img/kumo9-x24/MCLava.png", //25 kumo9-x24 missile residue
	"img/money_star.png", //26 money generator facility
	"img/tech_star.png", //27 researcher facility
	"img/moneybag.png", //28 money factory (hotbar)
	"img/power_star.png", //29 power plant
	"adwaitalegacy/battery-full-charging.png", //30 power plant (hotbar)
	"adwaitalegacy/battery-full.png", //31 full battery icon
	"adwaitalegacy/battery-caution.png", //32 Insufficient energy level icon
	"img/kumo9-x24/kumolaser.png", //33 Kumo x24 laser icon (LOL hotbar)
	"img/kumo9-x24/positron.png" //34 Kumo x24 positron icon (LOL hotbar)
};

//InitialIMGID, InitialHP, Team, ZIndex, damage, unit_type, inithitdiameter, timeout, requirepowerlevel
const int32_t NUMINFO[MAX_TID][9] = {
	{ 0,  5000,  TEAMID_ALLY, 0,    0,             UNITTYPE_FACILITY, 210,    0,  0}, //0 Earth
	{ 1, 10000, TEAMID_ENEMY, 0,    0,             UNITTYPE_FACILITY, 210,    0,  0}, //1 Zundamon Star
	{ 2,   200, TEAMID_ENEMY, 1,    0,                 UNITTYPE_UNIT, 100,    0,  0}, //2 Zundamon
	{ 3,   300, TEAMID_ENEMY, 1,    0,                 UNITTYPE_UNIT, 100,    0,  0}, //3 Spicy Zundamon
	{ 4,   500, TEAMID_ENEMY, 1,    0,                 UNITTYPE_UNIT, 100,    0,  0}, //4 Mad Zundamon
	{ 5,    70, TEAMID_ENEMY, 2,    0, UNITTYPE_BULLET_INTERCEPTABLE,  50,  300,  0}, //5 Zundamon space mine
	{ 6,  5000,  TEAMID_ALLY, 0,    0,             UNITTYPE_FACILITY, 200,    0,  2}, //6 Pit
	{ 7, 15000,  TEAMID_ALLY, 0,    0,             UNITTYPE_FACILITY, 200,    0, 10}, //7 Fort
	{ 8,    20,  TEAMID_ALLY, 2,    0, UNITTYPE_BULLET_INTERCEPTABLE,  10,  300,  0}, //8 Missile
	{-1,     0,  TEAMID_ALLY, 2,    1,               UNITTYPE_BULLET, 100,    0,  0}, //9 AllyExplosion
	{-1,     0, TEAMID_ENEMY, 2,    1,               UNITTYPE_BULLET, 100,    0,  0}, //10 EnemyExplosion
	{-1,     0,  TEAMID_NONE, 2,    1,               UNITTYPE_BULLET, 100,    0,  0}, //11 Explosion
	{ 9,     0,  TEAMID_ALLY, 2,   10,               UNITTYPE_BULLET,  10,  700,  0}, //12 AllyBullet
	{10,     0, TEAMID_ENEMY, 2,   20,               UNITTYPE_BULLET,  20,  800,  0}, //13 EnemyBullet (Edamame)
	{-1,     0, TEAMID_ENEMY, 2,   15,               UNITTYPE_BULLET, 600,  100,  0}, //14 Enemy Zunda laser
	{11,  2500,  TEAMID_ALLY, 1,    0,                 UNITTYPE_UNIT,  50,    0,  0}, //15 Kumo9-x24-robot
	{12,   500, TEAMID_ENEMY, 1,    0,                 UNITTYPE_UNIT, 100,    0,  0}, //16 Kamikaze zundamon
	{24,    70,  TEAMID_ALLY, 2,    0, UNITTYPE_BULLET_INTERCEPTABLE,  10,  700,  0}, //17 Kumo9 x24 missile
	{25,     0,  TEAMID_ALLY, 0,    5,               UNITTYPE_BULLET, 100,  100,  0}, //18 kumo9 x24 missile residue
	{26,  3000,  TEAMID_ALLY, 0,    0,             UNITTYPE_FACILITY, 210,    0, 15}, //19 Money generater facility
	{27,  3000,  TEAMID_ALLY, 0,    0,             UNITTYPE_FACILITY, 210,    0,  4}, //20 researchment facility
	{29,  5000,  TEAMID_ALLY, 0,    0,             UNITTYPE_FACILITY, 210,    0,  0}, //21 power plant
	{-1,     0,  TEAMID_ALLY, 2,    5,               UNITTYPE_BULLET, 600,  500,  0}, //22 Kumo9-x24-robot-laser
	{-1,     0,  TEAMID_ALLY, 2,   50,               UNITTYPE_BULLET,   0,   20,  0}  //23 Kumo9-x24-robot-pcanon
};

//MaxSpeeds, damage
const double DBLINFO[MAX_TID] = {
	0,   //0
	0.05, //1
	1,   //2
	1.5, //3
	0.5, //4
	2.3,   //5
	0,   //6
	0,   //7
	2,   //8
	0,   //9
	0,   //10
	0,   //11
	3.0, //12
	5.0, //13
	0,   //14
	1.0, //15
	0.5, //16
	2.5, //17
	0,   //18
	0,   //19
	0,   //20
	0,   //21
	0,   //22
	0    //23
};

const char* getlocalizedstring(int32_t stringid) {
	if(!is_range(stringid, 0, MAX_STRINGS - 1) ) {
		die("getlocalizedstring(): bad stringid passed.\n");
		return NULL;
	}
	switch(LangID) {
	case LANGID_JP:
		return JP_STRINGS[stringid];
	default:
		return EN_STRINGS[stringid];
	}
}

void lookup(obj_type_t i, LookupResult_t* r) {
	if(!is_range(i, 0, MAX_TID - 1)) {
		die("lookup() failed: bad tid: %d\n", i);
		return;
	}
	r->initimgid = NUMINFO[i][0];
	r->inithp = NUMINFO[i][1];
	r->teamid = NUMINFO[i][2];
	r->maxspeed = DBLINFO[i];
	r->zindex = NUMINFO[i][3];
	r->damage = NUMINFO[i][4];
	r->objecttype = (facility_type_t)NUMINFO[i][5];
	r->inithitdiameter = NUMINFO[i][6];
	r->timeout = NUMINFO[i][7];
	r->requirepower = NUMINFO[i][8];
}

void check_data() {
//Check data
	for(uint8_t i = 0; i < MAX_TID; i++) {
		if(!is_range(NUMINFO[i][0], -1, IMAGE_COUNT - 1)) {
			die("info.c: check_data(): bad initimgid value on tid %d\n", i);
			return;
		}
		if(!is_range(NUMINFO[i][1], 0, 65535)) {
			die("info.c: check_data(): bad inithp value on tid %d\n", i);
			return;
		}
		if(!is_range(NUMINFO[i][2], -1, 127)) {
			die("info.c: check_data(): bad teamid value on tid %d\n", i);
			return;
		}
		if(!is_range(NUMINFO[i][3], 0, MAX_ZINDEX - 1)) {
			die("info.c: check_data(): bad zindex value on tid %d\n", i);
			return;
		}
		if(!is_range(NUMINFO[i][4], 0, 65535)) {
			die("info.c: check_data(): bad damage value on tid %d\n", i);
			return;
		}
		if(!is_range(NUMINFO[i][5], 0, 127)) {
			die("info.c: check_data(): bad unitid value on tid %d\n", i);
			return;
		}
		if(!is_range(NUMINFO[i][6], 0, 65535)) {
			die("info.c: check_data(): bad inithitdiameter value on tid %d\n", i);
			return;
		}
		if(!is_range(NUMINFO[i][7], 0, 65535)) {
			die("info.c: check_data(): bad timeout value on tid %d\n", i);
			return;
		}
		if(!is_range(NUMINFO[i][8], 0, 255) ) {
			die("info.c: check_data(): bad reqpwrlevel value on tid %d\n", i);
			return;
		}
		if(!is_range_number(DBLINFO[i], -5, 5)) {
			die("info.c: check_data(): bad maxspeed value on tid %d\n", i);
			return;
		}
	}
	for(uint8_t i = 0; i < PLAYABLE_CHARACTERS_COUNT; i++) {
		if(!is_range(PLAYABLE_INFORMATION[i][0], 0, MAX_TID - 1) ) {
			die("info.c: check_data(): bad tid specified for playable character %d\n", i);
			return;
		}
		if(!is_range(PLAYABLE_INFORMATION[i][1], 0, IMAGE_COUNT - 1) ) {
			die("info.c: check_data(): bad imageid specified for playable character %d\n", i);
			return;
		}
		if(!is_range(PLAYABLE_INFORMATION[i][2], 0, IMAGE_COUNT - 1) ) {
			die("info.c: check_data(): bad imageid (dead) specified for playable character %d\n", i);
			return;
		}
		for(uint8_t j = 0; j < SKILL_COUNT; j++) {
			if(!is_range(SKILLICONIDS[i][j], -1, IMAGE_COUNT - 1) ) {
				die("info.c: check_data(): bad imageid specified for playable character %d's skill (%d) icon\n", i, j);
				return;
			}
		}
	}
}

void lookup_playable(int32_t i, PlayableInfo_t *t) {
	if(!is_range(i, 0, PLAYABLE_CHARACTERS_COUNT - 1) ) {
		die("lookup_playable(): bad id passed!\n");
		return;
	}
	t->associatedtid = (obj_type_t)PLAYABLE_INFORMATION[i][0];
	t->portraitimgid = PLAYABLE_INFORMATION[i][1];
	t->portraitimg_dead_id = PLAYABLE_INFORMATION[i][2];
	t->skillcooldowns = SKILLCOOLDOWNS[i];
	t->skillimageids = SKILLICONIDS[i];
	t->skillinittimers = SKILL_INIT_TIMERS[i];
	t->skillranges = SKILL_RANGES[i];
}

const char* getlocalizeditemdesc(int32_t did) {
	if(!is_range(did,0 ,ITEM_COUNT - 1) ) {
		die("getlocalizeditemdesc(): bad did passed: %d\n", did);
		return NULL;
	}
	if(LangID == LANGID_JP) {
		return ITEMDESC_JP[did];
	}
	return ITEMDESC_EN[did];
}


const char* getlocalizedcharactername(int32_t did) {
	if(!is_range(did,0 , MAX_TID - 1) ) {
		die("getlocalizedcharactername(): bad did passed: %d\n", did);
		return NULL;
	}
	if(LangID == LANGID_JP) {
		return JP_TID_NAMES[did];
	}
	return EN_TID_NAMES[did];
}

//Return 1 if tid is playable character id.
int32_t is_playable_character(obj_type_t tid) {
	if(tid == TID_KUMO9_X24_ROBOT) {
		return 1;
	}
	return 0;
} 
