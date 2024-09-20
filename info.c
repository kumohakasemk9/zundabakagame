/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by Gtk4

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

info.c: game object information and localization info
*/

#include "main.h"

extern uint8_t LangID;

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
	"現在使用出来ません"
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
	"Unavailable"
};

//Item description strings: jp
const char* ITEMDESC_JP[ITEM_COUNT] = {
	"入渠: 周囲のユニットを回復する施設を設置します",
	"要塞: 周囲の敵を迎撃する施設を設置します",
	"ずんだ畑: 囮ずんだもんをを生成する施設を設置します",
	"攻撃的なずんだ畑: ミサイルを発射するずんだもんを生成する施設を設置します",
	"速度改良: 自機の速度を早くします"
};

//Item description strings: en
const char* ITEMDESC_EN[ITEM_COUNT] = {
	"Pit: Places the facility that recovers nearby units.",
	"Fort: Places the facility that attacks nearby enemies.",
	"ZundaFarm: Places the facility that makes decoy zundamons.",
	"Offensive ZundaFarm: Place the facility that makes zundamons who launches missiles.",
	"Speed Upgrade: Boost your speed."
};

//Hotbar Item Image ID LUT
const uint8_t ITEMIMGIDS[ITEM_COUNT] = {
	14, //pit
	15, //fort
	23, //decoy
	26, //offensive zundafarm
	16  //upgrade
};

//Item Values
const uint16_t ITEMPRICES[ITEM_COUNT] = {
	10, //pit
	20, //fort
	15, //decoy
	60, //offensive zundafarm
	30  //upgrade
};

//Facility TID vs Item ID
const int8_t FTIDS[ITEM_COUNT] = {
	TID_PIT, //pit
	TID_FORT, //fort
	TID_DECOYGEN, //decoy
	TID_ZUNDAMISSILESHIPGEN, //offensive zundafarm
	-1 //upgrade item, this is not facility
};

//Item cooldown defaults
const uint16_t ITEMCOOLDOWNS[ITEM_COUNT] = {
	1000, //pit
	2000, //fort
	1000, //decoy
	5000, //offensive zundamon farm
	4000  //upgrade
};

//Playable character skill cooldowns
const int16_t SKILLCOOLDOWNS[PLAYABLE_CHARACTERS_COUNT][SKILL_COUNT] = {
	{1000, 3000, 6000} //kumo9-x24 cooldowns (kumohakasemk9)
};

//Playable character skill icon ids LUT
const int8_t SKILLICONIDS[PLAYABLE_CHARACTERS_COUNT][SKILL_COUNT] = {
	{-1, -1, -1} //kumo9-x24 skill icons (kumohakasemk9)
};

//Playable character information (tid, portraitimgid, dead portrait)
const int8_t PLAYABLE_INFORMATION[PLAYABLE_CHARACTERS_COUNT][3] = {
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
	"adwaitalegacy/dialog-warning.png", //9 bullet
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
	"img/zunda_ally.png",  //21 friend decoy zundamon
	"img/zunda_decoy_src.png", //22 friend decoy zundamon factory
	"adwaitalegacy/mail-mark-important.png", //23 friend decoy zundamon factory (hotbar)
	"img/zunda-atk.png", //24 missile zundamon
	"img/zunda_missile_src.png", //25 missile zundamon factory
	"img/zunda_missile_src_ico.png" //26 missile zundamon factory (Icon)
};

//InitialIMGID, InitialHP, Team, ZIndex, damage, unit_type, inithitdiameter, timeout
const int16_t NUMINFO[MAX_TID][8] = {
	{ 0, 10000,  TEAMID_ALLY, 0,  0,             UNITTYPE_FACILITY, 210,    0}, //0 Earth
	{ 1, 20000, TEAMID_ENEMY, 0,  0,             UNITTYPE_FACILITY, 210,    0}, //1 Zundamon Star
	{ 2,   100, TEAMID_ENEMY, 1,  0,                 UNITTYPE_UNIT, 100,    0}, //2 Zundamon
	{ 3,   300, TEAMID_ENEMY, 1,  0,                 UNITTYPE_UNIT, 100,    0}, //3 Spicy Zundamon
	{ 4,   500, TEAMID_ENEMY, 1,  0,                 UNITTYPE_UNIT, 100,    0}, //4 Mad Zundamon
	{ 5,    70, TEAMID_ENEMY, 2,  0, UNITTYPE_BULLET_INTERCEPTABLE,  50,  300}, //5 Zundamon space mine
	{ 6,  5000,  TEAMID_ALLY, 0,  0,             UNITTYPE_FACILITY, 200,    0}, //6 Pit
	{ 7,  7000,  TEAMID_ALLY, 0,  0,             UNITTYPE_FACILITY, 200,    0}, //7 Fort
	{ 8,    20,  TEAMID_ALLY, 2,  0, UNITTYPE_BULLET_INTERCEPTABLE,  10,  300}, //8 Missile
	{-1,     0,  TEAMID_ALLY, 2,  1,               UNITTYPE_BULLET, 100,    0}, //9 AllyExplosion
	{-1,     0, TEAMID_ENEMY, 2,  1,               UNITTYPE_BULLET, 100,    0}, //10 EnemyExplosion
	{-1,     0,  TEAMID_NONE, 2,  1,               UNITTYPE_BULLET, 100,    0}, //11 Explosion
	{ 9,     0,  TEAMID_ALLY, 2, 10,               UNITTYPE_BULLET,  10,  700}, //12 AllyBullet
	{10,     0, TEAMID_ENEMY, 2, 20,               UNITTYPE_BULLET,  20,  800}, //13 EnemyBullet (Edamame)
	{-1,     0, TEAMID_ENEMY, 2, 15,               UNITTYPE_BULLET,   0,   50}, //14 Enemy Zunda laser
	{11,  5000,  TEAMID_ALLY, 1,  0,                 UNITTYPE_UNIT,  50,    0}, //15 Kumo9-x24-robot
	{12,   500, TEAMID_ENEMY, 1,  0,                 UNITTYPE_UNIT, 100,    0}, //16 Kamikaze zundamon
	{22,  6000,  TEAMID_ALLY, 0,  0,             UNITTYPE_FACILITY, 200,    0}, //17 Decoy zundamon generator
	{21,   400,  TEAMID_ALLY, 1,  0,                 UNITTYPE_UNIT, 100, 6000}, //18 Decoy zundamon
	{24,   300,  TEAMID_ALLY, 1,  0,                 UNITTYPE_UNIT, 100, 3000}, //19 Missile Zundamon
	{25,  3000,  TEAMID_ALLY, 0,  0,             UNITTYPE_FACILITY, 200,    0}  //20 Missile zundamon generator
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
	0,   //17
	1.5, //18
	1.4  //19
};

const char* getlocalizedstring(uint8_t stringid) {
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
		die("lookup() failed: bad tid: %d", i);
		return;
	}
	r->initimgid = (int8_t)NUMINFO[i][0];
	r->inithp = (uint16_t)NUMINFO[i][1];
	r->teamid = (teamid_t)NUMINFO[i][2];
	r->maxspeed = DBLINFO[i];
	r->zindex = (uint8_t)NUMINFO[i][3];
	r->damage = (uint16_t)NUMINFO[i][4];
	r->objecttype = (facility_type_t)NUMINFO[i][5];
	r->inithitdiameter = (uint16_t)NUMINFO[i][6];
	r->timeout = (uint16_t)NUMINFO[i][7];
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

void lookup_playable(int8_t i, PlayableInfo_t *t) {
	if(!is_range(i, 0, PLAYABLE_CHARACTERS_COUNT - 1) ) {
		die("lookup_playable(): bad id passed!\n");
		return;
	}
	t->associatedtid = (obj_type_t)PLAYABLE_INFORMATION[i][0];
	t->portraitimgid = (int8_t)PLAYABLE_INFORMATION[i][1];
	t->portraitimg_dead_id = (int8_t)PLAYABLE_INFORMATION[i][2];
	t->skillcooldowns = (int16_t*)SKILLCOOLDOWNS[i];
	t->skillimageids = (int8_t*)SKILLICONIDS[i];
}

const char* getlocalizeditemdesc(uint8_t did) {
	if(!is_range(did,0 ,ITEM_COUNT - 1) ) {
		die("getlocalizeditemdesc(): bad did passed: %d\n", did);
		return NULL;
	}
	if(LangID == LANGID_JP) {
		return ITEMDESC_JP[did];
	}
	return ITEMDESC_EN[did];
}
