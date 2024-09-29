/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by Gtk4

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

aiproc.c: Character AI
*/

#include "main.h"

extern GameObjs_t Gobjs[MAX_OBJECT_COUNT];
extern gamestate_t GameState;
extern int32_t Money;
extern int32_t PlayingCharacterID;
extern int32_t StateChangeTimer;
extern int32_t MapTechnologyLevel;
extern int32_t MapEnergyLevel;
extern int32_t MapRequiredEnergyLevel;

void procai() {
	//AI proc
	uint16_t earthcount = 0;
	uint16_t enemybasecount = 0;
	uint16_t tecstar_count = 0;
	uint16_t powerplant_cnt = 0;
	int32_t tmpreqpowerlevel = 0;
	for(uint16_t i = 0; i < MAX_OBJECT_COUNT; i++) {
		if(Gobjs[i].tid == TID_NULL) {continue;}
		LookupResult_t srcinfo;
		lookup(Gobjs[i].tid, &srcinfo);
		//Process dead character (skip when inithp = 0, it means they are invulnerable)
		if(Gobjs[i].hp == 0 && srcinfo.inithp != 0) {
			if(srcinfo.objecttype == UNITTYPE_FACILITY) {
				//Facility will make big explosion if destroyed
				int32_t r = add_character(TID_EXPLOSION, Gobjs[i].x, Gobjs[i].y);
				Gobjs[r].hitdiameter = 500;
			} else if(Gobjs[i].tid != TID_ALLYBULLET && Gobjs[i].tid != TID_ENEMYBULLET) {
				//other than bullets and planets, explodes when dead
				add_character(TID_EXPLOSION, Gobjs[i].x, Gobjs[i].y);
			}
			//Check if playable character is dead
			if(i == PlayingCharacterID) {
				PlayingCharacterID = -1;
				GameState = GAMESTATE_DEAD;
				StateChangeTimer = 1000;
			}
			Gobjs[i].tid = TID_NULL;
			continue; //Skip process for this object
		}
		//Process speed
		Gobjs[i].x = constrain_number(Gobjs[i].x + Gobjs[i].sx, 0, MAP_WIDTH);
		Gobjs[i].y = constrain_number(Gobjs[i].y + Gobjs[i].sy, 0, MAP_HEIGHT);
		//Missile, Bullet will be erased when they reach to map edge
		if(Gobjs[i].tid == TID_ALLYBULLET || Gobjs[i].tid == TID_MISSILE || Gobjs[i].tid == TID_ENEMYBULLET) {
			if(Gobjs[i].x == 0 || Gobjs[i].y == 0 || Gobjs[i].x == MAP_WIDTH || Gobjs[i].y == MAP_HEIGHT) {
				Gobjs[i].tid = TID_NULL;
				continue; //skip further process
			}
		}
		//Process timers
		Gobjs[i].timer0 = constrain_i32(Gobjs[i].timer0 - 1, 0, 65535);
		Gobjs[i].timer1 = constrain_i32(Gobjs[i].timer1 - 1, 0, 65535);
		Gobjs[i].timer2 = constrain_i32(Gobjs[i].timer2 - 1, 0, 65535);
		Gobjs[i].timer3 = constrain_i32(Gobjs[i].timer3 - 1, 0, 65535);
		//If timeout != 0, decrease it and if it reaches to 0, delete object
		if(Gobjs[i].timeout != 0) {
			Gobjs[i].timeout--;
			if(Gobjs[i].timeout == 0) {
				//TID_MISSILE, TID_ENEMYZUNDAMONMINE , TID_ZUNDADECOY will explode when timeout
				switch(Gobjs[i].tid) {
				case TID_MISSILE:
					add_character(TID_ALLYEXPLOSION, Gobjs[i].x, Gobjs[i].y); //Explode
				break;
				case TID_ZUNDAMONMINE:
					add_character(TID_ENEMYEXPLOSION, Gobjs[i].x, Gobjs[i].y); //Explode
				break;
				}
				Gobjs[i].tid = TID_NULL;
				continue;
			}
		}
		//Calculate required Power
		tmpreqpowerlevel += srcinfo.requirepower;
		switch(Gobjs[i].tid) {
		case TID_EARTH:
			earthcount++;
			//Apply recovery according to MapTechnologyLevel
			Gobjs[i].hp = constrain_number(Gobjs[i].hp + MapTechnologyLevel, 0, srcinfo.inithp);
			break;
		case TID_ENEMYBASE:
			enemybasecount++;
			//Generate random zundamon every 20 sec
			if(Gobjs[i].timer0 == 0) {
				Gobjs[i].timer0 = 2000;
				if(is_range_number(Gobjs[i].hp, 10000, 20000) ) {
					add_character(TID_ZUNDAMON1, Gobjs[i].x, Gobjs[i].y);
				} else if(is_range_number(Gobjs[i].hp, 5000, 9999) ) {
					add_character(TID_ZUNDAMON2, Gobjs[i].x, Gobjs[i].y);
				} else {
					add_character(TID_ZUNDAMON3, Gobjs[i].x, Gobjs[i].y);
				}
				//There is 20% chance of kamikaze zundamon swawn
				if(randint(0, 100) < 20) {
					add_character(TID_ZUNDAMON_KAMIKAZE, Gobjs[i].x, Gobjs[i].y);
				}
			}
			//Stop moving if they got close to earth
			if(is_range(Gobjs[i].aiming_target, 0, MAX_OBJECT_COUNT - 1) ) {
				if(get_distance(Gobjs[i].aiming_target, i) < 200) {
					Gobjs[i].sx = 0;
					Gobjs[i].sy = 0;
				}
			}
			break;
		case TID_ZUNDAMON1:
			//Generate zundamon mine every 5 sec
			if(Gobjs[i].timer0 == 0) {
				Gobjs[i].timer0 = 500;
				add_character(TID_ZUNDAMONMINE, Gobjs[i].x, Gobjs[i].y);
			}
			//Follow nearest object
			Gobjs[i].aiming_target = find_nearest_unit(i, DISTANCE_INFINITY, UNITTYPE_UNIT | UNITTYPE_FACILITY);
			set_speed_for_following(i);
			break;
		case TID_ZUNDAMON2:
		case TID_ZUNDAMON3:
			//Generate zundamon mine every 3 sec
			if(Gobjs[i].timer0 == 0) {
				Gobjs[i].timer0 = 300;
				add_character(TID_ZUNDAMONMINE, Gobjs[i].x, Gobjs[i].y);
			}
			//Targets random unit
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
			set_speed_for_following(i);
			break;
		case TID_ZUNDAMONMINE:
		case TID_ZUNDAMON_KAMIKAZE:
		case TID_KUMO9_X24_MISSILE:
			//Follow enemy persistently
			if(!is_range(Gobjs[i].aiming_target, 0, MAX_OBJECT_COUNT - 1) ) {
				//if aiming_target is not set, find new object to aim
				if(Gobjs[i].tid == TID_ZUNDAMONMINE) {
					Gobjs[i].aiming_target = find_nearest_unit(i, 1500, UNITTYPE_BULLET_INTERCEPTABLE | UNITTYPE_UNIT | UNITTYPE_FACILITY);
				} else if(Gobjs[i].tid == TID_ZUNDAMON_KAMIKAZE) {
					Gobjs[i].aiming_target = find_nearest_unit(i, DISTANCE_INFINITY, UNITTYPE_UNIT);
				} else if (Gobjs[i].tid == TID_KUMO9_X24_MISSILE) {
					Gobjs[i].aiming_target = find_random_unit(i, DISTANCE_INFINITY, UNITTYPE_UNIT | UNITTYPE_FACILITY);
				}
			} else if(Gobjs[Gobjs[i].aiming_target].tid == TID_NULL) {
				Gobjs[i].aiming_target = OBJID_INVALID;
			}
			set_speed_for_following(i);
			break;
		case TID_ENEMYZUNDALASER:
		case TID_KUMO9_X24_LASER:
			//Damage aimed target if they are in range
			if(is_range(Gobjs[i].aiming_target, 0, MAX_OBJECT_COUNT - 1) ) {
				if(Gobjs[Gobjs[i].aiming_target].tid != TID_NULL && get_distance(i, Gobjs[i].aiming_target) < Gobjs[i].hitdiameter / 2) {
					//If aiming target is valid and in range, attack them
					damage_object(Gobjs[i].aiming_target, srcinfo.damage, 65535);
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
		case TID_ALLYEXPLOSION:
		case TID_ENEMYEXPLOSION:
		case TID_EXPLOSION:
			Gobjs[i].hitdiameter = (uint16_t)constrain_i32(Gobjs[i].hitdiameter - 3, 0, 65535);
			if(Gobjs[i].hitdiameter == 0) {
				Gobjs[i].tid = TID_NULL;
				continue;
			}
			break;
		case TID_KUMO9_X24_ROBOT:
			if(Gobjs[i].timer1 != 0) {
				//If timer0 is not 0, generate missile every 50mS
				if(Gobjs[i].timer1 % 50 == 0) {
					add_character(TID_KUMO9_X24_MISSILE, Gobjs[i].x, Gobjs[i].y);
				}
			}
			if(Gobjs[i].timer2 == 499) {
				int32_t r = add_character(TID_KUMO9_X24_LASER, Gobjs[i].x, Gobjs[i].y);
				Gobjs[r].parentid = i;
				//g_print("procai(): Kumo9x24 (%d): laser object generated.\n", i);
			}
			break;
		case TID_RESEARCHMENT_CENTRE:
			if(MapRequiredEnergyLevel < MapEnergyLevel) {tecstar_count++;} //Count number of researchment centre
			break;
		case TID_MONEY_GENE:
			//Money gene will give 2 parts per 10 sec
			if(Gobjs[i].timer0 == 0 && MapRequiredEnergyLevel < MapEnergyLevel) {
				Gobjs[i].timer0 = 1000;
				Money += 2;
			}
			break;
		case TID_POWERPLANT:
			powerplant_cnt++;
			break;
		}
		//Hitdetection proc
		for(uint16_t j = 0; j < MAX_OBJECT_COUNT; j++) {
			if(i == j) { continue; }
			if(Gobjs[j].tid == TID_NULL) { continue; }
			LookupResult_t dstinfo;
			lookup(Gobjs[j].tid, &dstinfo);
			double d = get_distance(i, j);
			//normal hitdetection
			if(d < ((Gobjs[i].hitdiameter + Gobjs[j].hitdiameter) / 2)) {
				//g_print("gamesys.c: hitdetect: source %d target %d\n", i, j);
				if(procobjhit(i, j, srcinfo, dstinfo) == FALSE) {
					//g_print("gametick(): id %d (TID=%d) erased because of collision\n", i, Gobjs[i].tid);
					Gobjs[i].tid = TID_NULL;
					break; //if FALSE returned, erase source object and skip further hit detection
				}
			}
			//radar detection
			switch(Gobjs[i].tid) {
			case TID_EARTH:
				if(d < (EARTH_RADAR_DIAM / 2) ) {
					//Earth radar
					if(dstinfo.teamid == TEAMID_ENEMY && dstinfo.inithp != 0 ) {
						//Enemy incoming (except unkillable thing)
						if(Gobjs[i].timer0 == 0) {
							//spawn bullet
							add_character(TID_ALLYBULLET, Gobjs[i].x, Gobjs[i].y);
							Gobjs[i].timer0 = 10; //give delay
						}
					}
					if(dstinfo.teamid == TEAMID_ALLY && (dstinfo.objecttype & UNITTYPE_UNIT ) ) {
						//Ally incoming (recover them)
						Gobjs[j].hp = constrain_number(Gobjs[j].hp + 1, 0, dstinfo.inithp);
					}
				}
				break;
			case TID_FORT:
				if(d < (FORT_RADAR_DIAM / 2) && MapRequiredEnergyLevel < MapEnergyLevel) {
					//Fort radar
					if(dstinfo.teamid == TEAMID_ENEMY && dstinfo.inithp != 0 ) {
						//Enemy incoming
						if(Gobjs[i].timer0 == 0) {
							//spawn bullet and aim the enemy
							add_character(TID_ALLYBULLET, Gobjs[i].x, Gobjs[i].y);
							Gobjs[i].timer0 = 10; //give delay
						}
						if(Gobjs[i].timer1 == 0) {
							//spawn bullet and aim the enemy
							add_character(TID_MISSILE, Gobjs[i].x, Gobjs[i].y);
							Gobjs[i].timer1 = 100; //give delay
						}
					}
				}
				break;
			case TID_PIT:
				if(d < (PIT_RADAR_DIAM / 2) && MapRequiredEnergyLevel < MapEnergyLevel) {
					//Pit radar
					if(dstinfo.teamid == TEAMID_ALLY && (dstinfo.objecttype & UNITTYPE_UNIT) ) {
						//Recover Ally
						Gobjs[j].hp = constrain_number(Gobjs[j].hp + 2, 0, dstinfo.inithp);
					}
				}
				break;
			case TID_ZUNDAMON2:
			case TID_ZUNDAMON3:
				//Zundamon 2 and 3 will spawn bullet if obj is close
				if(d < (ZUNDAMON2_RADAR_DIAM / 2) ) {
					if(dstinfo.teamid == TEAMID_ALLY && dstinfo.inithp != 0 ) {
						//Enemy incoming
						if(Gobjs[i].timer1 == 0) {
							//spawn bullet and aim the enemy
							add_character(TID_ENEMYBULLET, Gobjs[i].x, Gobjs[i].y);
							Gobjs[i].timer1 = 5;
						}
					}
				}
				if(d < (ZUNDAMON3_RADAR_DIAM / 2) && Gobjs[i].tid == TID_ZUNDAMON3) {
					if(dstinfo.teamid == TEAMID_ALLY && dstinfo.inithp != 0 ) {
						if(Gobjs[i].timer2 == 0) {
							//spawn laser and aim the enemy
							int32_t r = add_character(TID_ENEMYZUNDALASER, Gobjs[i].x, Gobjs[i].y);
							Gobjs[r].parentid = (int16_t)i;
							Gobjs[i].timer2 = 1000;
						}
					}
				}
				break;
			case TID_ENEMYBASE:
				if(d < ENEMYBASE_RADAR_DIAM / 2) {
					if(dstinfo.teamid == TEAMID_ALLY && dstinfo.inithp != 0 ) {
						//g_print("gametick(): ENEMYBASE(%d): ally unit approaching.\n", i);
						if(Gobjs[i].timer1 == 0) {
							//spawn laser and aim the enemy
							int32_t r = add_character(TID_ENEMYZUNDALASER, Gobjs[i].x, Gobjs[i].y);
							Gobjs[r].parentid = (int16_t)i;
							Gobjs[i].timer1 = 500;
							//g_print("gametick(): ENEMYBASE(%d): spawn laser.\n", i);
						}
						//Spawn bullet
						if(Gobjs[i].timer2 == 0) {
							add_character(TID_ENEMYBULLET, Gobjs[i].x, Gobjs[i].y);
							Gobjs[i].timer2 = 10;
						}
					}
				}
				break;
			case TID_KUMO9_X24_ROBOT:
				//playable is auto-attack enemy within radar diam
				double m = 1.0 + (MapTechnologyLevel * 100);
				if(d < (PLAYABLE_AUTOMACHINEGUN_DIAM + m) / 2) {
					if(dstinfo.teamid == TEAMID_ENEMY && dstinfo.inithp != 0 ) {
						//Enemy incoming
						if(Gobjs[i].timer0 == 0) {
							//spawn bullet
							add_character(TID_ALLYBULLET, Gobjs[i].x, Gobjs[i].y);
							Gobjs[i].timer0 = 10;
						}
					}
				}
				break;
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
		}
	}
}

//Called when Gobjs[dst] object is close (within Gobjs[dst].hitdiameter) to Gobjs[src] object
//Return FALSE if you want to delete source object
gboolean procobjhit(int32_t src, int32_t dst, LookupResult_t srcinfo, LookupResult_t dstinfo) {
	if(!is_range(src, 0, MAX_OBJECT_COUNT - 1) || !is_range(dst, 0, MAX_OBJECT_COUNT - 1)) {
		die("damage_object(): Bad parameter!\n");
		return FALSE;
	}
	if(dstinfo.inithp != 0 && srcinfo.teamid != dstinfo.teamid) {
		//Damage if different team id and target initial hp is not 0
		if(Gobjs[src].tid == TID_MISSILE || Gobjs[src].tid == TID_ZUNDAMONMINE || Gobjs[src].tid == TID_ZUNDAMON_KAMIKAZE || Gobjs[src].tid == TID_KUMO9_X24_MISSILE) {
			//Explode missile if hit to enemy object (unit and facility)
			if(dstinfo.objecttype == UNITTYPE_FACILITY || dstinfo.objecttype == UNITTYPE_UNIT) {
				if(Gobjs[src].tid == TID_MISSILE) {
					add_character(TID_ALLYEXPLOSION, Gobjs[src].x, Gobjs[src].y);
				} else if(Gobjs[src].tid == TID_ZUNDAMONMINE) {
					add_character(TID_ENEMYEXPLOSION, Gobjs[src].x, Gobjs[src].y);
				} else if(Gobjs[src].tid == TID_ZUNDAMON_KAMIKAZE) {
					int32_t r = add_character(TID_ENEMYEXPLOSION, Gobjs[src].x, Gobjs[src].y);
					Gobjs[r].hitdiameter = 500;
					Gobjs[r].damage = 2.5;
				} else if(Gobjs[src].tid == TID_KUMO9_X24_MISSILE) {
					//Kumo9 x24 missile will put lava and destroy self
					add_character(TID_KUMO9_X24_MISSILE_RESIDUE, Gobjs[src].x, Gobjs[src].y);
				}
				return FALSE; //kill source object
				//g_print("procobjhit(): Missile %d exploded: hit with %d\n", src, dst);
			}
		} else if(Gobjs[src].damage != 0) {
			//Except missile and object has dealing damage
			damage_object(dst, Gobjs[src].damage, 65535);
			//preserve explosion or laser
			if(Gobjs[src].tid == TID_ENEMYZUNDALASER || Gobjs[src].tid == TID_ALLYEXPLOSION || Gobjs[src].tid == TID_ENEMYEXPLOSION || Gobjs[src].tid == TID_EXPLOSION || Gobjs[src].tid == TID_KUMO9_X24_MISSILE_RESIDUE || Gobjs[src].tid == TID_KUMO9_X24_LASER) {
				return TRUE;
			}
			return FALSE; //kill this source object if not laser nor explosion
		}
	}
	return TRUE;
}

void damage_object(int32_t i, double damage, int32_t hpmax) {
	if(!is_range(i, 0, MAX_OBJECT_COUNT - 1)) {
		die("damage_object(): Bad parameter!\n");
		return;
	}
	Gobjs[i].hp = constrain_number(Gobjs[i].hp - damage , 0, hpmax);
	if(Gobjs[i].hp == 0) {
		//Object Dead
		//g_print("damage_object(): Object %d is dead.\n", i);
		//If enemy unit is dead, give money to ally team
		switch(Gobjs[i].tid) {
		case TID_ZUNDAMON1:
			Money += 3;
			break;
		case TID_ZUNDAMON2:
			Money += 7;
			break;
		case TID_ZUNDAMON3:
			Money += 13;
			break;
		case TID_ENEMYBASE:
			Money += 50;
			break;
		}
	}
}
