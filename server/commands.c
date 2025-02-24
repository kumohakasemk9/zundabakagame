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

void stop_command(int);
void ban_command(char*, int);
int GetUserOpLevel(int);
int GetUIDByName(char*);

extern int ProgramExit;
extern userinfo_t *UserInformations;
extern cliinfo_t C[MAX_CLIENTS];

void ExecServerCommand(char* cmd, int cid) {
	//Op level 1: reset, 2: ban welcome, 3: adduser deluser op 4: stop
	if(strcmp("?stop", cmd) == 0) {
		stop_command(cid);

	/*} else if(memcmp("?adduser ", cmd, 9) == 0) {
		//adduser command
		if(GetUserOpLevel(cid) >= 3) {
			AddUser(&cmd[9]);
		} else {
			SendPrivateChat(SERVER_EVENT, cid, "Insufficient permission.");
			Log(cid, "Bad op level\n");
		}

	} else if(memcmp("?deluser ", cmd, 9) == 0) {
		//Delete user command
		int uid = GetUIDByName(&cmd[9]);
		if(GetUserOpLevel(cid) >= 3) {
			if(uid != -1) {
				Log(cid, "User %s will be removed.\n", UserInformations[uid].usr);
				UserInformations[uid].usr[0] = 0;
			} else {
				SendPrivateChat(SERVER_EVENT, cid, "Insufficient permission.");
				Log(cid, "Specified user not found.\n");
			}
		} else {
			Log(cid, "Bad op level.\n");
		}*/
	
	} else if(memcmp("?ban ", cmd, 5) == 0) {
		ban_command(&cmd[5], cid);
	/*} else if(memcmp("?test", cmd) == 0) {
		//Test command
		SendJoinPacket(15);
	*/} else {
		SendPrivateChat(SERVER_EVENT, cid, "Bad server command");
		Log(cid, "Bad command.\n");
	}
}

void stop_command(int cid) {
	//stop command
	if(GetUserOpLevel(cid) >= 4) {
		Log(cid, "Server stop request\n");
		ProgramExit = 1;
	} else {
		SendPrivateChat(SERVER_EVENT, cid, "Insufficient permission.");
		Log(cid, "Bad op level\n");
	}
}

void ban_command(char *p, int cid) {
	//Ban command
	int uid = GetUIDByName(&p[5]);
	if(GetUserOpLevel(cid) >= 2) {
		if(uid != -1) {
			Log(cid, "User %s will be banned.\n", UserInformations[uid].usr);
			UserInformations[uid].ban = -1;
			//Disconnect if banned user is connected
			for(int i = 0; i < MAX_CLIENTS; i++) {
				if(C[i].uid == uid && C[i].fd != -1) {
					C[i].closereq = 1;
					break;
				}
			}
		} else {
			Log(cid, "Specified user not found.\n");
		}
	} else {
		SendPrivateChat(SERVER_EVENT, cid, "Insufficient permission.");
		Log(cid, "Bad op level.\n");
	}
}


