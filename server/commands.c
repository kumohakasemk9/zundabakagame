/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

commands.c: server commands
*/

#include "../inc/server.h"

#include <string.h>
#include <time.h>
#include <stdlib.h>

void stop_command(int);
void ban_command(char*, int);
void welcome_command(char*, int);
void chmod_command(char*, int);
int GetUserOpLevel(int);
int GetUIDByName(char*);
extern int ProgramExit;
extern userinfo_t *UserInformations;
extern cliinfo_t C[MAX_CLIENTS];


void ExecServerCommand(char* cmd, int cid) {
	//Op level 1: reset, 2: ban welcome op 3: stop
	if(strcmp("?stop", cmd) == 0) {
		stop_command(cid);
	} else if(memcmp("?ban ", cmd, 5) == 0) {
		ban_command(&cmd[5], cid);
	} else if(memcmp("?welcome ", cmd, 9) == 0) {
		welcome_command(&cmd[9], cid);
	} else if(memcmp("?chmod ", cmd, 7) == 0) {
		chmod_command(&cmd[4], cid);
	/*} else if(memcmp("?test", cmd) == 0) {
		//Test command
		SendJoinPacket(15);
	*/} else {
		SendPrivateChat(SERVER_EVENT, cid, "Bad server command");
		Log(cid, "Bad server command.\n");
	}
}

void stop_command(int cid) {
	//stop command
	if(GetUserOpLevel(cid) >= 3) {
		Log(cid, "Server stop request\n");
		ProgramExit = 1;
	} else {
		SendPrivateChat(SERVER_EVENT, cid, "Insufficient permission.");
		Log(cid, "stop_command(): Bad op level\n");
	}
}

void chmod_command(char* p, int cid) {
	//op command
	if(GetUserOpLevel(cid) < 2) {
		SendPrivateChat(SERVER_EVENT, cid, "Insufficient permission.");
		Log(cid, "welcome_command(): Bad op level.\n");
		return;
	}
	
	//Split command
	char uname[UNAME_SIZE];
	int opl = -1;
	size_t ptr = 0;
	for(int i = 0; i < 2; i++) {
		char *off = strchr(&p[ptr], ' ');
		size_t l;
		if(off == NULL) {
			if(i != 1) {
				Log(cid, "chmod_command(): bad parameter!\n");
				break;
			}
			l = strnlen(&p[ptr], NET_CHAT_LIMIT);
		} else {
			l = off - &p[ptr];
		}
		if(i == 0) {
			if(l >= UNAME_SIZE) {
				Log(cid, "chmod_command(): too long username!\n");
				break;
			}
			CopyAsString(uname, &p[ptr], l);
		} else if(i == 1) {
			if(l >= 2) {
				Log(cid, "chmod_command(): too long oplevel!\n");
				break;
			}
			char st[2];
			CopyAsString(st, &p[ptr], l);
			opl = atoi(st);
			if(opl < 0 || opl > 3) {
				Log(cid, "chmod_command(): bad op level!\n");
				break;
			}
		}
		ptr += l + 1;
	}
	if(opl == -1) {
		SendPrivateChat(SERVER_EVENT, cid, "usage: ?chmod username oplevel");
		return;
	}

	int uid = GetUIDByName(uname);
	if(uid != -1) {
		Log(cid, "op: %s(%d) -> %d\n", UserInformations[uid].usr, uid, opl);
		UserInformations[uid].op = opl;
	} else {
		SendPrivateChat(SERVER_EVENT, cid, "User not found.");
		Log(cid, "welcome_command(): Specified user not found.\n");
	}

}

void welcome_command(char* p, int cid) {
	//Welcome command
	if(GetUserOpLevel(cid) < 2) {
		SendPrivateChat(SERVER_EVENT, cid, "Insufficient permission.");
		Log(cid, "welcome_command(): Bad op level.\n");
		return;
	}
	
	//Check param
	if(strlen(p) == 0) {
		SendPrivateChat(SERVER_EVENT, cid, "usage: ?welcome username");
		Log(cid, "welcome_command(): Bad command usage.\n");
		return;
	}

	int uid = GetUIDByName(p);
	if(uid != -1) {
		Log(cid, "welcome: %s(%d)\n", UserInformations[uid].usr, uid);
		UserInformations[uid].banreason[0] = 1;
	} else {
		SendPrivateChat(SERVER_EVENT, cid, "User not found.");
		Log(cid, "welcome_command(): Specified user not found.\n");
	}
}

void ban_command(char *p, int cid) {
	//Ban command
	if(GetUserOpLevel(cid) < 2) {
		SendPrivateChat(SERVER_EVENT, cid, "Insufficient permission.");
		Log(cid, "ban_command(): Bad op level.\n");
		return;
	}

	//Split options
	char banreason[BAN_REASON_SIZE];
	char uname[UNAME_SIZE];
	int duration;
	banreason[0] = 0;
	size_t ptr = 0;
	for(int i = 0; i < 3; i++) {
		char* off = strchr(&p[ptr], ' ');
		size_t l;
		if(off == NULL) {
			if(i != 2) {
				Log(cid, "ban_command(): bad parameter!\n");
				break;
			}
			l = strnlen(&p[ptr], NET_CHAT_LIMIT);
		} else {
			l = off - &p[ptr];
		}
		if(i == 0) {
			if(l >= UNAME_SIZE) {
				Log(cid, "ban_command(): too long username.\n");
				break;
			}
			CopyAsString(uname, &p[ptr], l);
		} else if(i == 2) {
			if(l >= BAN_REASON_SIZE) {
				Log(cid, "ban_command(): too long reason.\n");
				break;
			}
			CopyAsString(banreason, &p[ptr], l);
		} else if(i == 1) {
			char st[20];
			if(l >= 20) {
				Log(cid, "ban_command(): too long duration.\n");
				break;
			}
			CopyAsString(st, &p[ptr], l);
			if(strcmp(st, "forever") == 0) {
				duration = 0;
			} else {
				duration = atol(st); //If there's conversion error, it returns 0 and it will ban user forever, so number input bottom limit is 1 to avoid accident permanent ban
				if(duration < 1) {
					Log(cid, "ban_command(): bad duration.\n");
					break;
				}
			}
		}
		ptr += l + 1;
	}
	if(banreason[0] == 0) {
		SendPrivateChat(SERVER_EVENT, cid, "usage: ?ban username reason duration");
		return;
	}

	int uid = GetUIDByName(uname);
	if(uid != -1) {
		Log(cid, "ban: %s(%d) reason: %s duration: %ld\n", UserInformations[uid].usr, uid, banreason, duration);
		if(duration == 0) {
			UserInformations[uid].ban = 0;
		} else {
			UserInformations[uid].ban = duration + time(NULL);
		}
		strcpy(UserInformations[uid].banreason, banreason);
		//Disconnect if banned user is connected
		for(int i = 0; i < MAX_CLIENTS; i++) {
			if(C[i].uid == uid && C[i].fd != -1) {
				DisconnectWithReason(cid, "Account suspended/banned.");
				break;
			}
		}
	} else {
		SendPrivateChat(SERVER_EVENT, cid, "User not found.");
		Log(cid, "ban_command(): Specified user not found.\n");
	}
}


