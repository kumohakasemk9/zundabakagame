/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

server.h: server common definitions
*/

#define SERVER_EVENT -1
#define SIZE_NET_BUFFER 512
#define HTTP_LINE_LIMIT 256
#define SIZE_EVENT_BUFFER 8192

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include "network.h"

//User Information Struct
typedef struct {
	char usr[UNAME_SIZE];
	char pwd[PASSWD_SIZE];
	int op;
	int ban;
	char banreason[BAN_REASON_SIZE];
} userinfo_t;

//Client information struct
typedef struct  {
	int fd; //Client fd
	struct in_addr ip; //Client ip
	in_port_t port; //Client port
	int closereq; //Close request flag
	int inread; //MUTEX
	int bufcur; //current event buffer cursor
	int uid; //logged in user id, -1 if not logged in
	int rdlength;
	int timeouttimer;
	uint8_t rdbuf[SIZE_NET_BUFFER];
	int protocolid;
	char httpupgrade[HTTP_LINE_LIMIT];
	char httpfirstline[HTTP_LINE_LIMIT];
	char httpwskey[HTTP_LINE_LIMIT];
	int16_t playable_x;
	int16_t playable_y;
} cliinfo_t;

//command.c
void ExecServerCommand(char* ,int);

//server.c
void SendPrivateChat(int, int, char*);
void Log(int, const char*, ...);
int GetUserOpLevel(int);
int GetUIDByName(char*);
void DisconnectWithReason(int, char*);
ssize_t send_packet(void*, size_t, int);
int AddUser(char*);

//events.c
void EventBufferGC();
void AddEventCmd(uint8_t*, int, int);
void GetEvent(int);
void SendJoinPacket(int);
void SendLeavePacket(int);
