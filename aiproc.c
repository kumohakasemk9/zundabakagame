/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

aiproc.c: Character AI
*/

#include "inc/zundagame.h"

#include <string.h>
#include <math.h>

int32_t procobjhit(int32_t, int32_t, LookupResult_t, LookupResult_t);
void damage_object(int32_t, int32_t);
void set_speed_for_going_location(int32_t, double, double, double, double);
double get_distance(int32_t, int32_t);
void set_speed_for_following(int32_t, double);
int32_t find_nearest_unit(int32_t, int32_t, facility_type_t);
int32_t find_random_unit(int32_t, int32_t, facility_type_t);
void aim_earth(int32_t);

extern GameObjs_t Gobjs[MAX_OBJECT_COUNT];
extern gamestate_t GameState;
extern int32_t Money;
extern int32_t PlayingCharacterID;
extern int32_t StateChangeTimer;
extern int32_t MapTechnologyLevel;
extern int32_t MapEnergyLevel;
extern int32_t MapRequiredEnergyLevel;
extern langid_t LangID;
extern int32_t CharacterMove;
extern int32_t SMPStatus;
extern SMPPlayers_t SMPPlayerInfo[MAX_CLIENTS];
extern double DifATKGain;
extern int32_t SpawnRemain;
extern int32_t InitSpawnRemain;
int32_t TitleSkillTimer = 0;

void procai() {
	//AI proc
	//If game in title, random playable will use skill every 5 sec
	if(GameState == GAMESTATE_TITLE) {
		TitleSkillTimer++;
		if(TitleSkillTimer > 500) {
			TitleSkillTimer = 0;
			//Get playable character ids
			int32_t playablegids[MAX_OBJECT_COUNT];
			int32_t c = 0;
			for(int32_t i = 0; i < MAX_OBJECT_COUNT; i++) {
				if(is_playable_character(Gobjs[i].tid) ) {
					playablegids[c] = i;
					c++;
				}
			}
			if(c != 0) {
				int32_t selectedgid = playablegids[randint(0, c - 1)];
				int32_t skillid = randint(0, SKILL_COUNT - 1);
				int32_t initskillt = get_skillinittimer(Gobjs[selectedgid].tid, skillid);
				Gobjs[selectedgid].timers[1 + skillid] = constrain_i32(initskillt, 0, 500);
			}
		}
	}
	int32_t earthcount = 0;
	int32_t enemybasecount = 0;
	int32_t tecstar_count = 0;
	int32_t powerplant_cnt = 0;
	int32_t tmpreqpowerlevel = 0;
	for(int32_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		obj_type_t tid = Gobjs[i].tid;
		if(Gobjs[i].tid == TID_NULL) {continue;}
		LookupResult_t srcinfo;
		if(lookup(Gobjs[i].tid, &srcinfo) == -1) {
			return;
		}
		//Process dead character (skip when inithp = 0, it means they are invulnerable)
		if(Gobjs[i].hp == 0 && srcinfo.inithp != 0) {
			if(srcinfo.objecttype == UNITTYPE_FACILITY) {
				//Facility will make big explosion if destroyed
				int32_t r = add_character(TID_EXPLOSION, Gobjs[i].x, Gobjs[i].y, i);
				Gobjs[r].hitdiameter = 500;
			} else if(tid != TID_ALLYBULLET && tid != TID_ENEMYBULLET) {
				//other than bullets and planets, explodes when dead
				add_character(TID_EXPLOSION, Gobjs[i].x, Gobjs[i].y, i);
			}
			//Check if playable character is dead
			if(i == PlayingCharacterID) {
				PlayingCharacterID = -1;
				//If SpawnRemain is not zero, decrease it and respawn
				if(SpawnRemain > 0 || InitSpawnRemain == -1) {
					if(SpawnRemain > 0) {
						SpawnRemain--;
					}
					GameState = GAMESTATE_DEAD;
					StateChangeTimer = 1000;
				} else {
					//Orelse game over (if local, in multiplay all playables need to be dead)
					if(SMPStatus != NETWORK_LOGGEDIN) {
						GameState = GAMESTATE_GAMEOVER;
					}
				}
			} else {
				//If SMP remote player is dead, respawn after several times
				if(SMPStatus == NETWORK_LOGGEDIN && is_playable_character(tid) ) {
					for(int32_t j = 0; j < MAX_CLIENTS; j++) {
						if(SMPPlayerInfo[j].playable_objid == i) {
							//Set specific player's SMP character respawn timer if their respawn_remain is not 0 or -1.
							if(SMPPlayerInfo[j].respawn_remain > 0 || SMPPlayerInfo[j].respawn_remain == -1) {
								if(SMPPlayerInfo[j].respawn_remain > 0) {
									SMPPlayerInfo[j].respawn_remain--;
								}
								SMPPlayerInfo[j].respawn_timer = 1000;
								SMPPlayerInfo[j].playable_objid = -1;
							}
							break;
						}
					}
				}
			}
			Gobjs[i].tid = TID_NULL;
			continue; //Skip process for this object
		}
		//Process speed
		Gobjs[i].x = constrain_number(Gobjs[i].x + Gobjs[i].sx, 0, MAP_WIDTH);
		Gobjs[i].y = constrain_number(Gobjs[i].y + Gobjs[i].sy, 0, MAP_HEIGHT);
		//Missile, Bullet will be erased when they reach to map edge
		if(tid == TID_ALLYBULLET || tid == TID_MISSILE || tid == TID_ENEMYBULLET) {
			if(Gobjs[i].x == 0 || Gobjs[i].y == 0 || Gobjs[i].x == MAP_WIDTH || Gobjs[i].y == MAP_HEIGHT) {
				Gobjs[i].tid = TID_NULL;
				continue; //skip further process
			}
		}
		//Process timers
		for(uint8_t j = 0; j < CHARACTER_TIMER_COUNT; j++) {
			Gobjs[i].timers[j] = constrain_i32(Gobjs[i].timers[j] - 1, 0, 65535);
		}
		//If timeout != 0, decrease it and if it reaches to 0, delete object
		if(Gobjs[i].timeout != 0) {
			Gobjs[i].timeout--;
			if(Gobjs[i].timeout == 0) {
				//missiles, zundamine will explode when timeout
				obj_type_t tid = Gobjs[i].tid;
				if(tid == TID_MISSILE || tid == TID_KUMO9_X24_MISSILE) {
					add_character(TID_ALLYEXPLOSION, Gobjs[i].x, Gobjs[i].y, i); //Explode
				} else if(tid == TID_ZUNDAMONMINE) {
					add_character(TID_ENEMYEXPLOSION, Gobjs[i].x, Gobjs[i].y, i); //Explode
				}
				Gobjs[i].tid = TID_NULL;
				continue;
			}
		}
		//Calculate required Power
		tmpreqpowerlevel += srcinfo.requirepower;

		//AI Processes for each object types
		if(tid == TID_EARTH) {
			earthcount++;
			//Apply recovery according to MapTechnologyLevel
			Gobjs[i].hp = constrain_number(Gobjs[i].hp + MapTechnologyLevel, 0, srcinfo.inithp);
			//If in title, spawn ally every 10 sec
			if(GameState == GAMESTATE_TITLE && Gobjs[i].timers[1] == 0) {
				Gobjs[i].timers[1] = 1000;
				int32_t newallycid = randint(0, PLAYABLE_CHARACTERS_COUNT - 1);
				PlayableInfo_t newally;
				lookup_playable(newallycid, &newally);
				int32_t naobjid = add_character(newally.associatedtid, Gobjs[i].x, Gobjs[i].y, i);
				Gobjs[naobjid].timeout = 6000;
			}

		} else if(tid == TID_ENEMYBASE) {
			enemybasecount++;
			//Generate random zundamon every 20 sec, but every 10 sec if in title
			if(Gobjs[i].timers[0] == 0) {
				obj_type_t newetid;
				if(GameState != GAMESTATE_TITLE) {
					Gobjs[i].timers[0] = 2000;
					if(is_range_number(Gobjs[i].hp, 10000, 20000) ) {
						newetid = TID_ZUNDAMON1;
					} else if(is_range_number(Gobjs[i].hp, 5000, 9999) ) {
						newetid = TID_ZUNDAMON2;
					} else {
						newetid = TID_ZUNDAMON3;
					}
				} else {
					//Spawn random unit when title
					Gobjs[i].timers[0] = 1000;
					obj_type_t ETIDS[] = {TID_ZUNDAMON1, TID_ZUNDAMON2, TID_ZUNDAMON3};
					newetid = ETIDS[randint(0, 2)];
				}
				int32_t newgid = add_character(newetid, Gobjs[i].x, Gobjs[i].y, i);
				if(GameState == GAMESTATE_TITLE) {
					Gobjs[newgid].timeout = 6000;
				}
				//There is 20% chance of kamikaze zundamon swawn
				if(randint(0, 100) < 20) {
					add_character(TID_ZUNDAMON_KAMIKAZE, Gobjs[i].x, Gobjs[i].y, i);
				}
			}
			//Stop moving if they got close to earth
			if(is_range(Gobjs[i].aiming_target, 0, MAX_OBJECT_COUNT - 1) ) {
				if(get_distance(Gobjs[i].aiming_target, i) < 200) {
					Gobjs[i].sx = 0;
					Gobjs[i].sy = 0;
				}
			}
			
		} else if(tid == TID_ZUNDAMON1) {
			//Generate zundamon mine every 5 sec
			if(Gobjs[i].timers[0] == 0) {
				Gobjs[i].timers[0] = 500;
				add_character(TID_ZUNDAMONMINE, Gobjs[i].x, Gobjs[i].y, i);
			}
			//Follow nearest object
			Gobjs[i].aiming_target = find_nearest_unit(i, DISTANCE_INFINITY, UNITTYPE_UNIT | UNITTYPE_FACILITY);
			set_speed_for_following(i, UNIT_STALK_LIMIT);
			
		} else if (tid == TID_ZUNDAMON2 || tid == TID_ZUNDAMON3) {
			//Generate zundamon mine every 3 sec
			if(Gobjs[i].timers[0] == 0) {
				Gobjs[i].timers[0] = 300;
				add_character(TID_ZUNDAMONMINE, Gobjs[i].x, Gobjs[i].y, i);
			}
			//Targets random unit (if game is in title state, zundamons already targets playable)
			if(!is_range(Gobjs[i].aiming_target, 0, MAX_OBJECT_COUNT - 1) ) {
				if(Gobjs[i].tid == TID_ZUNDAMON2) {
					Gobjs[i].aiming_target = find_random_unit(i, DISTANCE_INFINITY, UNITTYPE_UNIT);
				} else if(Gobjs[i].tid == TID_ZUNDAMON3) {
					Gobjs[i].aiming_target = find_random_unit(i, DISTANCE_INFINITY, UNITTYPE_FACILITY);
				}
			} else if(Gobjs[Gobjs[i].aiming_target].tid == TID_NULL) {
				//g_print("gamesys.c: gametick(): zundamon2(%d): stalking object dead: %d\n", i, Gobjs[i].aiming_target);
				Gobjs[i].aiming_target = OBJID_INVALID;
			}
			set_speed_for_following(i, UNIT_STALK_LIMIT);
		
		} else if(tid == TID_ZUNDAMONMINE || tid == TID_ZUNDAMON_KAMIKAZE) {
			//Follow enemy persistently
			if(!is_range(Gobjs[i].aiming_target, 0, MAX_OBJECT_COUNT - 1) ) {
				//if aiming_target is not set, find new object to aim
				if(Gobjs[i].tid == TID_ZUNDAMONMINE) {
					Gobjs[i].aiming_target = find_nearest_unit(i, 1500, UNITTYPE_BULLET_INTERCEPTABLE | UNITTYPE_UNIT | UNITTYPE_FACILITY);
				} else if(Gobjs[i].tid == TID_ZUNDAMON_KAMIKAZE) {
					Gobjs[i].aiming_target = find_nearest_unit(i, DISTANCE_INFINITY, UNITTYPE_UNIT);
				}
			} else if(Gobjs[Gobjs[i].aiming_target].tid == TID_NULL) {
				Gobjs[i].aiming_target = OBJID_INVALID;
			}
			set_speed_for_following(i, 3);
			
		} else if(tid == TID_ENEMYZUNDALASER || tid == TID_KUMO9_X24_LASER) {
			//Damage aimed target if they are in range
			if(is_range(Gobjs[i].aiming_target, 0, MAX_OBJECT_COUNT - 1) ) {
				if(Gobjs[Gobjs[i].aiming_target].tid != TID_NULL && get_distance(i, Gobjs[i].aiming_target) < Gobjs[i].hitdiameter / 2) {
					//If aiming target is valid and in range, attack them
					damage_object(Gobjs[i].aiming_target, i);
				} else {
					//When out of range or , request to set new target
					Gobjs[i].aiming_target = OBJID_INVALID;
					//g_print("procai(): EnemyZundaLaser(%d): target dead or out of range\n", i);
				}
			} else {
				//If object is aiming invalid object, find new target
				Gobjs[i].aiming_target = find_nearest_unit(i, Gobjs[i].hitdiameter, UNITTYPE_FACILITY | UNITTYPE_UNIT);
				//g_print("procai(): EnemyZundaLaser(%d): finding new object\n", i);
			}
			if(is_range(Gobjs[i].parentid, 0, MAX_OBJECT_COUNT - 1) ) {
				if(Gobjs[Gobjs[i].parentid].tid == TID_NULL) {
					//Destroy laser object if parent dead.
					//g_print("gametick(): Enemy Zunda Laser(%d) deleted because object got out of range.\n", i);
					Gobjs[i].tid = TID_NULL;
					continue; //skip further process for this object
				} else {
					//follow parent
					Gobjs[i].x = Gobjs[Gobjs[i].parentid].x;
					Gobjs[i].y = Gobjs[Gobjs[i].parentid].y;
				}
			}
			continue; //Laser object has own damaging system, skip hit detection
			
		} else if(tid == TID_ALLYEXPLOSION || tid == TID_ENEMYEXPLOSION || tid == TID_EXPLOSION) {
			Gobjs[i].hitdiameter = (uint16_t)constrain_i32(Gobjs[i].hitdiameter - 3, 0, 65535);
			if(Gobjs[i].hitdiameter == 0) {
				Gobjs[i].tid = TID_NULL;
				continue;
			}
			
		} else if(tid == TID_KUMO9_X24_ROBOT) {
			//Missile Skill
			if(Gobjs[i].timers[1] != 0) {
				if(Gobjs[i].timers[1] % 50 == 0) {
					//If timer0 is not 0, generate missile
					add_character(TID_KUMO9_X24_MISSILE, Gobjs[i].x, Gobjs[i].y, i);
				}
			}
			//Laser skill
			if(Gobjs[i].timers[2] == 499) {
				add_character(TID_KUMO9_X24_LASER, Gobjs[i].x, Gobjs[i].y, i);
				//g_print("procai(): Kumo9x24 (%d): laser object generated.\n", i);
			}
			// Positron canon
			if(Gobjs[i].timers[3] != 0) {
				add_character(TID_KUMO9_X24_PCANNON, Gobjs[i].x, Gobjs[i].y, i);
				Gobjs[i].timers[3] = 0;
			}
			
		} else if(tid == TID_RESEARCHMENT_CENTRE) {
			if(MapRequiredEnergyLevel <= MapEnergyLevel) {tecstar_count++;} //Count number of researchment centre
			
		} else if(tid == TID_MONEY_GENE) {
			//Money gene will give 2 parts per 10 sec
			if(Gobjs[i].timers[0] == 0 && MapRequiredEnergyLevel <= MapEnergyLevel) {
				Gobjs[i].timers[0] = 1000;
				Money += 2;
			}
			
		} else if(tid == TID_POWERPLANT) {
			powerplant_cnt++;
			
		} else if(tid == TID_KUMO9_X24_PCANNON) {
			if(is_range(Gobjs[i].parentid, 0, MAX_OBJECT_COUNT - 1) && Gobjs[Gobjs[i].parentid].tid == TID_KUMO9_X24_ROBOT) {
				//if Parent is ok
				Gobjs[i].x = Gobjs[Gobjs[i].parentid].x;
				Gobjs[i].y = Gobjs[Gobjs[i].parentid].y;
				if(is_range(Gobjs[i].aiming_target, 0, MAX_OBJECT_COUNT - 1) && Gobjs[Gobjs[i].aiming_target].tid != TID_NULL) {
					//If target object is not dead
					if(Gobjs[i].timeout == 1) {
						//Make explosion (diam 600) on target before pcannon disappears
						int32_t r = add_character(TID_ALLYEXPLOSION, Gobjs[Gobjs[i].aiming_target].x, Gobjs[Gobjs[i].aiming_target].y, i);
						Gobjs[r].hitdiameter = 800;
						Gobjs[r].damage = 5;
					}
					damage_object(Gobjs[i].aiming_target, i);
				}
			} else {
				//Parent dead
				Gobjs[i].tid = TID_NULL;
			}
			continue; //This object don't need normal hitdetection
		}

		//If in title screen, enable playable object AI (follow enemy)
		if(GameState == GAMESTATE_TITLE && is_playable_character(tid) ) {
			if(!is_range(Gobjs[i].aiming_target, 0, MAX_OBJECT_COUNT - 1) ) {
				Gobjs[i].aiming_target = find_random_unit(i, DISTANCE_INFINITY, UNITTYPE_UNIT | UNITTYPE_FACILITY);
			} else if(Gobjs[Gobjs[i].aiming_target].tid == TID_NULL) {
				Gobjs[i].aiming_target = OBJID_INVALID;
			}
			set_speed_for_following(i, UNIT_STALK_LIMIT);
		}
		
		//Hitdetection proc
		for(uint16_t j = 0; j < MAX_OBJECT_COUNT; j++) {
			if(i == j) { continue; }
			if(Gobjs[j].tid == TID_NULL) { continue; }
			LookupResult_t dstinfo;
			if(lookup(Gobjs[j].tid, &dstinfo) == -1) {
				return;
			}
			double d = get_distance(i, j);
			//normal hitdetection
			if(d < ((Gobjs[i].hitdiameter + Gobjs[j].hitdiameter) / 2)) {
				//g_print("gamesys.c: hitdetect: source %d target %d\n", i, j);
				if(procobjhit(i, j, srcinfo, dstinfo) == 0) {
					//g_print("gametick(): id %d (TID=%d) erased because of collision\n", i, Gobjs[i].tid);
					Gobjs[i].tid = TID_NULL;
					break; //if 0 returned, erase source object and skip further hit detection
				}
			}
			//radar detection
			if(tid == TID_EARTH) {
				if(d < (EARTH_RADAR_DIAM / 2) ) {
					//Earth radar
					if(dstinfo.teamid == TEAMID_ENEMY && dstinfo.inithp != 0 ) {
						//Enemy incoming (except unkillable thing)
						if(Gobjs[i].timers[0] == 0) {
							//spawn bullet
							add_character(TID_ALLYBULLET, Gobjs[i].x, Gobjs[i].y, i);
							Gobjs[i].timers[0] = 10; //give delay
						}
					}
					if(dstinfo.teamid == TEAMID_ALLY && (dstinfo.objecttype & UNITTYPE_UNIT ) ) {
						//Ally incoming (recover them)
						if(Gobjs[j].hp != 0) { //BUGFIX: the earth tries to recover dead one, causes deathlog spam
							Gobjs[j].hp = constrain_number(Gobjs[j].hp + 1, 0, dstinfo.inithp);
						}
					}
				}
				
			} else if(tid == TID_FORT) {
				if(d < (FORT_RADAR_DIAM / 2) && MapRequiredEnergyLevel <= MapEnergyLevel) {
					//Fort radar
					if(dstinfo.teamid == TEAMID_ENEMY && dstinfo.inithp != 0 ) {
						//Enemy incoming
						if(Gobjs[i].timers[0] == 0) {
							//spawn bullet and aim the enemy
							add_character(TID_ALLYBULLET, Gobjs[i].x, Gobjs[i].y, i);
							Gobjs[i].timers[0] = 10; //give delay
						}
						if(Gobjs[i].timers[1] == 0) {
							//spawn bullet and aim the enemy
							add_character(TID_MISSILE, Gobjs[i].x, Gobjs[i].y, i);
							Gobjs[i].timers[1] = 100; //give delay
						}
					}
				}
				
			} else if(tid == TID_PIT) {
				if(d < (PIT_RADAR_DIAM / 2) && MapRequiredEnergyLevel <= MapEnergyLevel) {
					//Pit radar
					if(dstinfo.teamid == TEAMID_ALLY && (dstinfo.objecttype & UNITTYPE_UNIT) ) {
						//Recover Ally
						if(Gobjs[j].hp != 0) {
							Gobjs[j].hp = constrain_number(Gobjs[j].hp + 2, 0, dstinfo.inithp);
						}
					}
				}
				
			} else if(tid == TID_ZUNDAMON2 || tid == TID_ZUNDAMON3) {
				//Zundamon 2 and 3 will spawn bullet if obj is close
				if(d < (ZUNDAMON2_RADAR_DIAM / 2) ) {
					if(dstinfo.teamid == TEAMID_ALLY && dstinfo.inithp != 0 ) {
						//Enemy incoming
						if(Gobjs[i].timers[1] == 0) {
							//spawn bullet and aim the enemy
							add_character(TID_ENEMYBULLET, Gobjs[i].x, Gobjs[i].y, i);
							Gobjs[i].timers[1] = 5;
						}
					}
				}
				if(d < (ZUNDAMON3_RADAR_DIAM / 2) && Gobjs[i].tid == TID_ZUNDAMON3) {
					if(dstinfo.teamid == TEAMID_ALLY && dstinfo.inithp != 0 ) {
						if(Gobjs[i].timers[2] == 0) {
							//spawn laser and aim the enemy
							int32_t r = add_character(TID_ENEMYZUNDALASER, Gobjs[i].x, Gobjs[i].y, i);
							Gobjs[r].parentid = (int16_t)i;
							Gobjs[i].timers[2] = 500;
						}
					}
				}
				
			} else if(tid == TID_ENEMYBASE) {
				if(d < ENEMYBASE_RADAR_DIAM / 2) {
					if(dstinfo.teamid == TEAMID_ALLY && dstinfo.inithp != 0 ) {
						//g_print("gametick(): ENEMYBASE(%d): ally unit approaching.\n", i);
						if(Gobjs[i].timers[1] == 0) {
							//spawn laser and aim the enemy
							int32_t r = add_character(TID_ENEMYZUNDALASER, Gobjs[i].x, Gobjs[i].y, i);
							Gobjs[r].parentid = (int16_t)i;
							Gobjs[i].timers[1] = 500;
							//g_print("gametick(): ENEMYBASE(%d): spawn laser.\n", i);
						}
						//Spawn bullet
						if(Gobjs[i].timers[2] == 0) {
							add_character(TID_ENEMYBULLET, Gobjs[i].x, Gobjs[i].y, i);
							Gobjs[i].timers[2] = 10;
						}
					}
				}
				
			} else if(tid == TID_KUMO9_X24_ROBOT) {
				//playable is auto-attack enemy within radar diam
				double m = 1.0 + (MapTechnologyLevel * 100);
				if(d < (PLAYABLE_AUTOMACHINEGUN_DIAM + m) / 2) {
					if(dstinfo.teamid == TEAMID_ENEMY && dstinfo.inithp != 0 ) {
						//Enemy incoming
						if(Gobjs[i].timers[0] == 0) {
							//spawn bullet
							add_character(TID_ALLYBULLET, Gobjs[i].x, Gobjs[i].y, i);
							Gobjs[i].timers[0] = 10;
						}
					}
				}
			}
			
		}
	}
	//Determine map technology level
	MapTechnologyLevel = constrain_i32(tecstar_count, 0, 5);
	//Determine Power Level
	MapEnergyLevel = powerplant_cnt * 50;
	MapRequiredEnergyLevel = tmpreqpowerlevel;
	//Determine if gameclear or gameover
	if(GameState == GAMESTATE_PLAYING || GameState == GAMESTATE_DEAD) {
		//g_print("damage_object(): Game object Earth=%d Enemy=%d\n", earthcount, enemycount);
		//If Earth count is 0, we lose, if zundamon star count is 0, we win.
		if(earthcount == 0) { GameState = GAMESTATE_GAMEOVER; }
		else if(enemybasecount == 0) { GameState = GAMESTATE_GAMECLEAR; }
		//Init timer in both case
		if(earthcount == 0 || enemybasecount == 0) {
			StateChangeTimer = 0;
			CharacterMove = 0;
		}
	}
}

//Called when Gobjs[dst] object is close (within Gobjs[dst].hitdiameter) to Gobjs[src] object
//Return 0 if you want to delete source object
int32_t procobjhit(int32_t src, int32_t dst, LookupResult_t srcinfo, LookupResult_t dstinfo) {
	if(!is_range(src, 0, MAX_OBJECT_COUNT - 1) || !is_range(dst, 0, MAX_OBJECT_COUNT - 1)) {
		die("damage_object(): Bad parameter!\n");
		return 0;
	}
	if(dstinfo.inithp != 0 && srcinfo.teamid != dstinfo.teamid) {
		//Damage if different team id and target initial hp is not 0
		if(Gobjs[src].tid == TID_MISSILE || Gobjs[src].tid == TID_ZUNDAMONMINE || Gobjs[src].tid == TID_ZUNDAMON_KAMIKAZE || Gobjs[src].tid == TID_KUMO9_X24_MISSILE) {
			//Explode missile if hit to enemy object (unit and facility)
			if(dstinfo.objecttype == UNITTYPE_FACILITY || dstinfo.objecttype == UNITTYPE_UNIT) {
				if(Gobjs[src].tid == TID_MISSILE) {
					add_character(TID_ALLYEXPLOSION, Gobjs[src].x, Gobjs[src].y, src);
				} else if(Gobjs[src].tid == TID_ZUNDAMONMINE) {
					add_character(TID_ENEMYEXPLOSION, Gobjs[src].x, Gobjs[src].y, src);
				} else if(Gobjs[src].tid == TID_ZUNDAMON_KAMIKAZE) {
					int32_t r = add_character(TID_ENEMYEXPLOSION, Gobjs[src].x, Gobjs[src].y, src);
					Gobjs[r].hitdiameter = 500;
					Gobjs[r].damage = 2.5;
				} else if(Gobjs[src].tid == TID_KUMO9_X24_MISSILE) {
					//Kumo9 x24 missile will put lava and destroy self
					add_character(TID_KUMO9_X24_MISSILE_RESIDUE, Gobjs[src].x, Gobjs[src].y, src);
				}
				return 0; //kill source object
				//g_print("procobjhit(): Missile %d exploded: hit with %d\n", src, dst);
			}
		} else if(Gobjs[src].damage != 0) {
			//Except missile and object has dealing damage
			damage_object(dst, src);
			//preserve explosion or laser
			if(Gobjs[src].tid == TID_ENEMYZUNDALASER || Gobjs[src].tid == TID_ALLYEXPLOSION || Gobjs[src].tid == TID_ENEMYEXPLOSION || Gobjs[src].tid == TID_EXPLOSION || Gobjs[src].tid == TID_KUMO9_X24_MISSILE_RESIDUE || Gobjs[src].tid == TID_KUMO9_X24_LASER) {
				return 1;
			}
			return 0; //kill this source object if not laser nor explosion
		}
	}
	return 1;
}

//Damage object id dstid by object id srcid
void damage_object(int32_t dstid, int32_t srcid) {
	//Check parameter
	if(!is_range(dstid, 0, MAX_OBJECT_COUNT - 1) || !is_range(srcid, 0, MAX_OBJECT_COUNT - 1) ) {
		die("damage_object(): Bad parameter!\n");
		return;
	}
	if(Gobjs[dstid].hp == 0) {
		return; //If already died, return
	}

	//Lookup for info
	LookupResult_t srcinfo, dstinfo;
	if(lookup(Gobjs[srcid].tid, &srcinfo) == -1 || lookup(Gobjs[dstid].tid, &dstinfo) == -1) {
		return;
	}

	if(dstinfo.objecttype == UNITTYPE_FACILITY && GameState == GAMESTATE_TITLE) {
		return; //All facilities can not be damaged in title mode
	}

	//Decrease target HP
	double m = 1.00;
	//Apply DifATKGain if attacker is playable character
	if(is_playable_character(Gobjs[srcid].tid) ) {
		m = DifATKGain;
	}
	Gobjs[dstid].hp = constrain_number(Gobjs[dstid].hp - (Gobjs[srcid].damage * m) , 0, dstinfo.inithp);
	
	if(Gobjs[dstid].hp == 0) {
		//Object Dead
		//g_print("damage_object(): Object %d is dead.\n", i);
		int32_t killerid = Gobjs[srcid].srcid;
		if(killerid == PlayingCharacterID && Money < 500) {
			obj_type_t tid = Gobjs[dstid].tid;
			if(tid == TID_ZUNDAMON1) {
				Money += 5;
			
			} else if(tid == TID_ZUNDAMON2) {
				Money += 10;
			
			} else if(tid == TID_ZUNDAMON3) {
				Money += 20;
			
			} else if(tid == TID_ENEMYBASE) {
				Money += 100;
			
			} else if(tid == TID_ZUNDAMON_KAMIKAZE) {
				Money += 4;

			} else if(tid == TID_ZUNDAMONMINE) {
				Money += 1;
			
			}
		}
		
		//If playable dead, money will be half
		if(dstid == PlayingCharacterID) {
			Money = constrain_i32(Money, 0, Money / 2);
		}

		//Write deathlog if facilities or playable character
		if(dstinfo.objecttype == UNITTYPE_FACILITY || is_playable_character(Gobjs[dstid].tid) ) {
			//If enemy unit is killed by player, give money to ally team
			//printlog("Object %d (srcid=%d) killed %d.\n", srcid, killerid, dstid);
			//Minecraft style death log
			int32_t deathreasonid = 5;
			obj_type_t tid = Gobjs[srcid].tid;
			if(tid == TID_EXPLOSION || tid == TID_ALLYEXPLOSION || tid == TID_ENEMYEXPLOSION) {
				//blown up
				deathreasonid = 0;
					
			} else if(tid == TID_ENEMYBULLET) {
				//Zundad
				deathreasonid = 2;
					
			} else if(tid == TID_ENEMYZUNDALASER || tid == TID_KUMO9_X24_LASER || tid == TID_KUMO9_X24_MISSILE_RESIDUE) {
				//Burnt into crisp
				deathreasonid = 3;
					
			} else if(tid == TID_KUMO9_X24_PCANNON) {
				//Annihilated
				deathreasonid = 4;
					
			} else if(tid == TID_ALLYBULLET) {
				//Bee hive
				deathreasonid = 1;
				
			}
			char smpkiller[UNAME_SIZE + 2] = "", smpvictim[UNAME_SIZE + 2] = "";
			//If SMP, show smp username (if object is playable)
			if(SMPStatus == NETWORK_LOGGEDIN) {
				//If object SMP playable, get username of character owner
				for(int32_t i = 0; i < MAX_CLIENTS; i++) {
					if(is_playable_character(Gobjs[dstid].tid) && SMPPlayerInfo[i].playable_objid == dstid) {
						strcpy(smpvictim, " <");
						strcat(smpvictim, SMPPlayerInfo[i].usr);
						strcat(smpvictim, ">");
					}
					if(is_playable_character(Gobjs[srcid].tid) && SMPPlayerInfo[i].playable_objid == srcid) {
						strcpy(smpkiller, " <");
						strcat(smpkiller, SMPPlayerInfo[i].usr);
						strcat(smpkiller, ">");
					}
				}
			}

			if(GameState == GAMESTATE_TITLE) { 
				return; //No deathlog in title
			}
			//show death log
			if(is_range(killerid, 0, MAX_OBJECT_COUNT - 1) && is_range(Gobjs[killerid].tid, 0, MAX_TID - 1) ){
				int32_t killertid = Gobjs[killerid].tid;
				if(LangID == LANGID_JP) {
					chatf("%s%sは%s%sによって%s", getlocalizedcharactername(Gobjs[dstid].tid), smpvictim, getlocalizedcharactername(killertid), smpkiller, getlocalizeddeathreason(deathreasonid));
				} else {
					chatf("%s%s is %s by %s%s", getlocalizedcharactername(Gobjs[dstid].tid), smpvictim, getlocalizeddeathreason(deathreasonid), getlocalizedcharactername(killertid), smpkiller);
				}
				//printlog("Object %d is %s\n", killerid, EN_TID_NAMES[killertid]);
			} else {
				if(LangID == LANGID_JP) {
					chatf("%s%sは%s%s", getlocalizedcharactername(Gobjs[dstid].tid), smpvictim, getlocalizeddeathreason(deathreasonid) );
				} else {
					chatf("%s%s %s", getlocalizedcharactername(Gobjs[dstid].tid), smpvictim, getlocalizeddeathreason(deathreasonid) );
				}
			}
		}
	}
}

//Add character into game object buffer, returns -1 if fail.
//tid (character id), x, y(initial pos), parid (parent object id)
int32_t add_character(obj_type_t tid, double x, double y, int32_t parid) {
	int32_t newid = -1;
	//Get empty slot id
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		if(Gobjs[i].tid != TID_NULL) { continue; } //Skip occupied slot
		newid = i;
		break;
	}
	if(newid == -1) {
		die("gamesys.c: add_character(): Gameobj is full!\n");
		return 0;
	}
	//Add new character
	LookupResult_t t;
	if(lookup(tid, &t) == -1) {
		return 0;
	}
	Gobjs[newid].imgid = t.initimgid;
	Gobjs[newid].tid = tid;
	Gobjs[newid].x = x;
	Gobjs[newid].y = y;
	Gobjs[newid].hp = (double)t.inithp;
	Gobjs[newid].sx = 0;
	Gobjs[newid].sy = 0;
	Gobjs[newid].aiming_target = OBJID_INVALID;
	for(uint8_t i = 0; i < CHARACTER_TIMER_COUNT; i++) {
		Gobjs[newid].timers[i] = 0;
	}
	Gobjs[newid].hitdiameter = t.inithitdiameter;
	Gobjs[newid].timeout = t.timeout;
	Gobjs[newid].damage = t.damage;
	//set srcid
	if(is_range(parid, 0, MAX_OBJECT_COUNT - 1) ) {
		//Set src id to parid if Gobjs[parid] is UNITTYPE_UNIT or UNITTYPE_FACILITY
		//Otherwise inherit from parent.
		if(is_range(Gobjs[parid].tid, 0, MAX_TID - 1) ) {
			LookupResult_t t2;
			if(lookup(Gobjs[parid].tid, &t2) == -1) {
				return -1;
			}
			if(t2.objecttype == UNITTYPE_UNIT || t2.objecttype == UNITTYPE_FACILITY) {
				Gobjs[newid].srcid = parid;
			} else {
				Gobjs[newid].srcid = Gobjs[parid].srcid;
			}
		} else {
			Gobjs[newid].srcid = Gobjs[parid].srcid;
		}
	} else {
		Gobjs[newid].srcid = -1;
	}
	Gobjs[newid].parentid = parid;
	//MISSLIE, bullet and laser will try to aim closest enemy unit
	if(tid == TID_MISSILE || tid == TID_ALLYBULLET || tid == TID_ENEMYBULLET || tid == TID_KUMO9_X24_MISSILE) {
		//Set initial target
		int32_t r;
		if(tid == TID_KUMO9_X24_MISSILE) {
			r = find_random_unit(newid, KUMO9_X24_MISSILE_RANGE, UNITTYPE_FACILITY | UNITTYPE_UNIT);
		} else {
			r = find_nearest_unit(newid, DISTANCE_INFINITY, UNITTYPE_FACILITY | UNITTYPE_BULLET_INTERCEPTABLE | UNITTYPE_UNIT);
		}
		if(r != OBJID_INVALID) {
			Gobjs[newid].aiming_target = r;
			set_speed_for_following(newid, 3);
		} else {
			Gobjs[newid].tid = TID_NULL;
			//warn("add_character(): object destroyed, no target found.\n");
		}
	}
	if(tid == TID_EARTH) {
		if(GameState != GAMESTATE_TITLE) {
			Gobjs[newid].hp = 1; //For initial scene
		}
		
	} else if(tid == TID_ENEMYBASE) {
		//tid = 1, zundamon star
		if(GameState != GAMESTATE_TITLE) {
			//find earth and attempt to approach it
			aim_earth(newid);
			Gobjs[newid].hp = 1; //For initial scene
		}

	} else if(tid == TID_KUMO9_X24_PCANNON) {
		if(GameState != GAMESTATE_TITLE) {
			//Kumo9's pcanon aims nearest facility
			Gobjs[newid].aiming_target = find_nearest_unit(newid, KUMO9_X24_PCANNON_RANGE, UNITTYPE_FACILITY);
		} else {
			Gobjs[newid].aiming_target = find_nearest_unit(newid, KUMO9_X24_PCANNON_RANGE, UNITTYPE_FACILITY | UNITTYPE_UNIT);
		}

	}
	return newid;
}

//calculate distance between Gobjs[id1] and Gobjs[id2]
double get_distance(int32_t id1, int32_t id2){
	if(!is_range(id1, 0, MAX_OBJECT_COUNT - 1) || !is_range(id2, 0, MAX_OBJECT_COUNT - 1)) {
		die("gamesys.c: set_speed_for_following(): bad id passed!\n");
		return INFINITY;
	}
	if(Gobjs[id1].tid == TID_NULL || Gobjs[id2].tid == TID_NULL) {
		return INFINITY;
	}
	return get_distance_raw(Gobjs[id1].x, Gobjs[id1].y, Gobjs[id2].x, Gobjs[id2].y);
}

double get_distance_raw(double x1, double y1, double x2, double y2) {
	double dx = fabs(x1 - x2);
	double dy = fabs(y1 - y2);
	return sqrt((dx * dx) + (dy * dy)); // a*a = b*b + c*c
}

//calculate appropriate speed for following object to Gobjs[srcid], stop if object is closer to the target more than distlimit
void set_speed_for_following(int32_t srcid, double distlimit) {
	if(!is_range(srcid, 0, MAX_OBJECT_COUNT - 1)) {
		die("gamesys.c: set_speed_for_following(): bad srcid passed!\n");
		return;
	}
	int32_t targetid = Gobjs[srcid].aiming_target;
	if(!is_range(targetid, 0, MAX_OBJECT_COUNT - 1)) {
		Gobjs[srcid].sx = 0;
		Gobjs[srcid].sy = 0;
		//g_print("gamesys.c: set_speed_for_following(): id %d has bad target id %d.\n", srcid, targetid);
		return;
	}
	if(Gobjs[targetid].tid == TID_NULL) {
		Gobjs[srcid].sx = 0;
		Gobjs[srcid].sy = 0;
		//g_print("gamesys.c: set_speed_for_following(): id %d has target id %d, this object is dead.\n", srcid, targetid);
		return;
	}
	double dstx = Gobjs[targetid].x;
	double dsty = Gobjs[targetid].y;
	LookupResult_t t;
	if(lookup((uint8_t)Gobjs[srcid].tid, &t) == -1) {
		return;
	}
	double spdlimit = t.maxspeed;
	set_speed_for_going_location(srcid, dstx, dsty, spdlimit, distlimit);
	//print(f"line 499: ID={srcid} set_speed_for_following maxspeed={spdlimit} sx={sx} sy={sy}")
}

//set appropriate speed for Gobjs[srcid] to go to coordinate dstx, dsty from current coordinate of Gobjs[srcid]
//stop object if objects are closer less than dstlimit
void set_speed_for_going_location(int32_t srcid, double dstx, double dsty, double speedlimit, double distlimit) {
	if(!is_range(srcid, 0, MAX_OBJECT_COUNT - 1)) {
		die("set_speed_for_going_location(): bad srcid passed!\n");
		return;
	}
	double srcx = Gobjs[srcid].x;
	double srcy = Gobjs[srcid].y;
	double dx = fabs(dstx - srcx);
	double dy = fabs(dsty - srcy);
	//If objects are closer than dstlimit, stop
	if(sqrt( (dx * dx) + (dy * dy) ) < distlimit) {
		Gobjs[srcid].sx = 0;
		Gobjs[srcid].sy = 0;
		return;
	}
	double sx, sy;
	if(dx > dy) {
		double r = 0;
		if(dy != 0) { r = dy / dx; }
		sx = speedlimit;
		sy = r * speedlimit;
	} else {
		double r = 0;
		if(dx != 0) {r = dx / dy; }
		sy = speedlimit;
		sx = r * speedlimit;
	}
	if(dstx < srcx) { sx = -sx; }
	if(dsty < srcy) { sy = -sy; }
	Gobjs[srcid].sx = sx;
	Gobjs[srcid].sy = sy;
}


//find nearest object (different team) from Gobjs[srcid] within diameter finddist
//if object has same objecttype specified in va_list, they will be excluded
//returns OBJID_INVALID if not found
int32_t find_nearest_unit(int32_t srcid, int32_t finddist, facility_type_t cfilter) {
	double mindist = finddist / 2;
	int32_t t = OBJID_INVALID;
	if(!is_range(srcid, 0, MAX_OBJECT_COUNT - 1)) {
		die("find_nearest_unit(): bad srcid passed!\n");
		return -1;
	}
	//esrc = self.gobjs[srcid]
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		//e = self.gobjs[i]
		if(Gobjs[i].tid != TID_NULL && srcid != i) {
			//C = self.lookup(e.tid)
			LookupResult_t dstinfo;
			LookupResult_t srcinfo;
			if(lookup(Gobjs[i].tid, &dstinfo) == -1 || lookup(Gobjs[srcid].tid, &srcinfo) == -1) {
				return -1;
			}
			//if objects are in same team, exclude from searching
			if(dstinfo.teamid != srcinfo.teamid && (cfilter & dstinfo.objecttype) ) {
				double dist = get_distance(i, srcid);
				//find nearest object
				if(dist < mindist) {
					mindist = dist;
					t = i;
				}
			}
		}
	}
	//if(t != -1){
	//	g_print("gamesys.c: find_nearest_unit(): src=%d id=%d dist=%d\n", srcid, t, mindist);
	//}
	return t;
}

//find random object (different team) from Gobjs[srcid] within diameter finddist
//if object has same objecttype specified in va_list, they will be excluded
//returns OBJID_INVALID if not found
int32_t find_random_unit(int32_t srcid, int32_t finddist, facility_type_t cfilter) {
	if(!is_range(srcid, 0, MAX_OBJECT_COUNT - 1)) {
		die("find_nearest_unit(): bad srcid passed!\n");
		return -1;
	}
	uint16_t findobjs[MAX_OBJECT_COUNT];
	uint16_t oc = 0;
	//esrc = self.gobjs[srcid]
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		//e = self.gobjs[i]
		if(Gobjs[i].tid != TID_NULL && srcid != i) {
			//C = self.lookup(e.tid)
			LookupResult_t dstinfo;
			LookupResult_t srcinfo;
			if(lookup(Gobjs[i].tid, &dstinfo) == -1 || lookup(Gobjs[srcid].tid, &srcinfo) == -1) {
				return -1;
			}
			//if objects are in same team, exclude from searching
			//if object type is not in cfilter, exclude
			//if object im more far than finddist / 2, exclude
			if(dstinfo.teamid != srcinfo.teamid && (dstinfo.objecttype & cfilter) && get_distance(srcid, i) < finddist / 2) {
				findobjs[oc] = i;
				oc++;
			}
		}
	}
	if(oc == 0) {
		return OBJID_INVALID;
	} else {
		uint16_t selnum = (uint16_t)randint(0, oc - 1);
		//if(t != -1){
		//	g_print("gamesys.c: find_nearest_unit(): src=%d id=%d dist=%d\n", srcid, t, mindist);
		//}
		return findobjs[selnum];
	}
}

//Set object to aim the earth
void aim_earth(int32_t i) {
	//Gobjs[i].aiming_target = (int16_t)find_earth();
		for(uint16_t j = 0; j < MAX_OBJECT_COUNT; j++) {
			if(Gobjs[j].tid == TID_EARTH) {
				Gobjs[i].aiming_target = j;
				break;
			}
		}
	set_speed_for_following(i, 3);
}

