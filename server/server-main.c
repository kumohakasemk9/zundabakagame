#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include <openssl/evp.h>

#define SERVER_EVENT -1
#define LIMIT_ADD_EVENT 512
#define SIZE_EVENT_BUFFER 8192
#define SIZE_NET_BUFFER 512
#define HTTP_LINE_LIMIT 256
#define PROTOCOL_HTTP 0
#define PROTOCOL_WEBSOCKET 1
#define PROTOCOL_RAW 2

#include <inc/zunda-server.h>

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
} cliinfo_t;

int ServerSocket;
int ProgramExit = 0;
cliinfo_t C[MAX_CLIENTS];
uint8_t EventBuffer[SIZE_EVENT_BUFFER];
int EBptr = 0;
userinfo_t *UserInformations = NULL;
int UserCount = 0;
uint8_t ServerSalt[SALT_LENGTH];
FILE *LogFile;
int Timeout = 5000; //0 to disable timeout

void SendJoinPacket(int);
void SendLeavePacket(int);
void SendPrivateChat(int, int, char*);
void INTHwnd(int);
void AcceptNewClient();
void ClientReceive(int);
void AddEventCmd(uint8_t*, int, int);
void AddEvent(uint8_t*, int, int);
void GetEvent(int);
void LoginWithHash(uint8_t*, int, int);
void DisconnectWithReason(int, char*);
void SendGreetingsPacket(int);
void EventBufferGC();
void Log(int, const char*, ...);
ssize_t GetEventPacketSize(uint8_t*, int);
void SendUserLists(int);
void recv_packet_handler(uint8_t*, size_t, int);
void recv_http_handler(char*, int);
void recv_websock_handler(uint8_t*, size_t, uint8_t, int, uint8_t*, int);
ssize_t send_packet(void*, size_t, int);
ssize_t send_websocket_packet(void*, size_t, uint8_t, int);
void ExecServerCommand(char* ,int);
int GetUserOpLevel(int);
int AddUser(char*);
int GetUIDByName(char*);

int main(int argc, char *argv[]) {
	int portnum = 25566;
	char *passwdfile = "server_passwd";
	//Process parameters
	for(int i = 1; i < argc; i++) {
		if(strcmp(argv[i], "--port") == 0) {
			if(i + 1 < argc) {
				int t = atoi(argv[i + 1]);
				if(1 <= t && t <= 65535) {
					portnum = t;
				} else {
					printf("Bad port number.\n");
				}
			} else {
				printf("You need to specify port number!!\n");
				return 1;
			}
		} else if(strcmp(argv[i], "--passfile") == 0) {
			if(i + 1 < argc) {
				passwdfile = argv[i + 1];
			} else {
				printf("You need to specify password file!!\n");
				return 1;
			}
		}
	}

	//Generate salt for user credential cipher
	for(int i = 0; i < SALT_LENGTH; i++) {
		ServerSalt[i] = random() % 0x100;
	}
	for(int i = 0; i < MAX_CLIENTS; i++) { C[i].fd = -1; }

	//Install signal
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa) );
	sa.sa_handler = INTHwnd;
	if(sigaction(SIGINT, &sa, NULL) == -1) {
		printf("Sigaction failed.\n");
		return 1;
	}

	//Make socket, set options
	ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(ServerSocket == -1) {
		perror("socket");
		return 1;
	}
	int opt = 1;
	setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt) );
	if(fcntl(ServerSocket, F_SETFL, O_NONBLOCK) == -1) {
		printf("Making non biock socket fail.\n");
		return 1;
	}

	//Bind
	struct sockaddr_in adr = {
		.sin_family = AF_INET,
		.sin_port = htons(portnum),
		.sin_addr.s_addr = INADDR_ANY
	};
	if(bind(ServerSocket, (struct sockaddr*)&adr, sizeof(adr) ) == -1) {
		perror("bind");
		close(ServerSocket);
		return 1;
	}

	//load user list
	//username:password:banreason:banexpiretime:oplevel:
	FILE *f_pwdfile = fopen(passwdfile, "r");
	if(f_pwdfile == NULL) {
		printf("Can not load %s.\n", passwdfile);
		close(ServerSocket);
		return 1;
	}
	int line = 1;
	while(1) {
		char b[8192];
		if(fgets(b, sizeof(b) - 1, f_pwdfile) == NULL) {
			break;
		}
		line++;
		b[8191] = 0; //for additional security
		if(strlen(b) == 0) { continue; }
		if(AddUser(b) != 0) {
			printf("at line %d\n", line);
		}
		line++;
	}
	fclose(f_pwdfile);
	
	if(UserCount == 0) {
		printf("Could not read users db.\n");
		close(ServerSocket);
		return 1;
	}

	//Listen
	if(listen(ServerSocket, 3) == -1) {
		perror("listen");
		close(ServerSocket);
		free(UserInformations);
		return 1;
	}

	//Open Log File for Appeding
	LogFile = fopen("log.txt", "a");
	if(LogFile == NULL) {
		perror("Can not open log.txt for writing");
		close(ServerSocket);
		free(UserInformations);
		return 1;
	}

	//Server loop
	printf("Server started at *:%d\n", portnum);
	while(ProgramExit == 0) {
		AcceptNewClient();
		for(int i = 0; i < MAX_CLIENTS; i++) {
			if(C[i].fd != -1) {
				ClientReceive(i);
				//Process close request
				if(C[i].closereq == 1) {
					Log(i, "Closing connection.\n");
					if(C[i].protocolid = PROTOCOL_WEBSOCKET) {
						send_websocket_packet("", 0, 8, i);
					}
					close(C[i].fd);
					C[i].fd = -1;
				}
				//Process timeout
				if(Timeout != 0 && C[i].fd != -1) {
					if(C[i].timeouttimer > Timeout) {
						Log(i, "Timed out.\n");
						DisconnectWithReason(i, "Server timeout.");
					} else {
						C[i].timeouttimer++;
					}
				}
			}
		}
		EventBufferGC();
		usleep(1);
	}
	//Finish
	printf("Closing server.\n");
	fclose(LogFile);
	close(ServerSocket);
	free(UserInformations);
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd != -1) {
			DisconnectWithReason(i, "Server closed.");
		}
	}
	return 0;
}

int AddUser(char* line) {
	char *offs[5];
	size_t lens[5];
	const size_t SIZELIMS[] = {UNAME_SIZE, PASSWD_SIZE, BAN_REASON_SIZE, 32, 16};
	size_t o = 0;
	for(int i = 0; i < 5; i++) {
		char *t = strchr(&line[o], ':');
		if(t == NULL) {
			printf("AddUser(): format error.\n");
			return -1;
		}
		size_t l = t - &line[o];
		lens[i] = l;
		offs[i] = &line[o];
		if(l >= SIZELIMS[i]) {
			printf("AddUser(): Too long column\n");
			return -1;
		}
		o += l + 1;
	}

	//Duplication check
	char t_uname[UNAME_SIZE];
	memcpy(t_uname, offs[0], lens[0]);
	t_uname[lens[0] ] = 0;
	if(GetUIDByName(t_uname) != -1) {
		printf("Duplicate user detect.\n");
		return -2;
	}

	//Append data to list
	int aid = -1;
	if(UserInformations == NULL) {
		UserInformations = malloc(sizeof(userinfo_t) );
		if(UserInformations == NULL) {
			printf("malloc failed.\n");
			return -3;
		}
		aid = 0;
		UserCount++;
	} else {
		//If usr field starts with null char, it can be overwritten
		for(int i = 0; i < UserCount; i++) {
			if(UserInformations[i].usr[0] == 0) {
				aid = i;
				break;
			}
		}
		//If every user fields are filled, create new.
		if(aid == -1) {
			userinfo_t *t = realloc(UserInformations, sizeof(userinfo_t) * (UserCount + 1) );
			if(t == NULL) {
				printf("realloc failed.\n");
				return -3;
			}
			UserInformations = t;
			aid = UserCount;
			UserCount++;
		}
	}
	//Store username, password, banreason
	char *outs[] = {UserInformations[aid].usr, UserInformations[aid].pwd, UserInformations[aid].banreason};
	for(int i = 0; i < 3; i++) {
		memcpy(outs[i], offs[i], lens[i]);
		outs[i][lens[i] ] = 0;
	}
	//Store op, ban
	UserInformations[aid].op = 0;
	UserInformations[aid].ban = 0;
	int *outs2[] = {(int*)&UserInformations[aid].ban, &UserInformations[aid].op};
	for(int i = 0; i < 2; i++) {
		char t[32];
		memcpy(t, offs[i + 3], lens[i + 3]);
		t[lens[i + 3] ] = 0;
		*outs2[i] = atoi(t);
	}
	printf("User add: %s, op=%d, banexpire=%d\n", UserInformations[aid].usr, UserInformations[aid].op, UserInformations[aid].ban);
	return 0;
}

void INTHwnd(int) {
	ProgramExit = 1;
}

int GetUIDByName(char* n) {
	for(int i = 0; i < UserCount; i++) {
		if(memcmp(UserInformations[i].usr, n, strlen(n) ) == 0) {
			return i;
		}
	}
	return -1;
}

void Log(int cid, const char* ptn, ...) {
	struct timeval t;
	double td;
	gettimeofday(&t, NULL);
	td = t.tv_sec + ( (double)t.tv_usec / 1000000.0);
	printf("[%.3f]", td);
	fprintf(LogFile, "[%.3f]", td);
	if(0 <= cid && cid <= MAX_CLIENTS - 1 && C[cid].fd != -1) {
		printf("<%d", cid);
		fprintf(LogFile, "<%d/%s/", cid, inet_ntoa(C[cid].ip) );
		if(0 <= C[cid].uid && C[cid].uid <= UserCount) {
			printf("/%s", UserInformations[C[cid].uid].usr);
			fprintf(LogFile, "/%s", UserInformations[C[cid].uid].usr);
		}
		printf(">");
		fprintf(LogFile, ">");
	}
	va_list varg;
	va_start(varg, ptn);
	vprintf(ptn, varg);
	va_end(varg);
	va_start(varg, ptn);
	vfprintf(LogFile, ptn, varg);
	va_end(varg);
}

void SendPrivateChat(int cid, int destcid, char* chat) {
	if(!(-1 <= cid && cid <= MAX_CLIENTS) ) {
		printf("SendPrivateChat(): invalid cid passed.\n");
		return;
	}
	if(!(0 <= destcid && destcid <= MAX_CLIENTS) ) {
		printf("SendPrivateChat(): invalid destcid passed.\n");
		return;
	}
	uint8_t t[SIZE_NET_BUFFER];
	size_t cl = strlen(chat);
	ev_chat_t hdr = {
		.evtype = EV_CHAT,
		.dstcid = destcid,
		.clen = htons(cl)
	};
	size_t evsiz = cl + sizeof(hdr);
	if(evsiz >= SIZE_NET_BUFFER) {
		printf("SendPrivateChat(): EV_CHAT packet overflow.\n");
		return;
	}
	memcpy(t, &hdr, sizeof(hdr) );
	memcpy(&t[sizeof(hdr)], chat, cl);
	AddEvent(t, evsiz, cid);
}

void SendJoinPacket(int cid) {
	uint8_t t[UNAME_SIZE + sizeof(ev_hello_t)];
	if(!(0 <= cid && cid <= MAX_CLIENTS) ) {
		printf("SendJoinPacket(): invalid cid passed.\n");
		return;
	}
	int uid = C[cid].uid;
	if(!(0 <= uid && uid <= UserCount) ) {
		printf("SetupJoinPacket(): good cid, but not logged in.\n");
		return;
	}
	ev_hello_t ev = {
		.evtype = EV_HELLO,
		.cid = cid,
		.uname_len = strlen(UserInformations[uid].usr) + 1
	};
	memcpy(&t, &ev, sizeof(ev_hello_t) );
	memcpy(&t[sizeof(ev_hello_t)], UserInformations[uid].usr, ev.uname_len);
	AddEvent(t, ev.uname_len + sizeof(ev_hello_t), SERVER_EVENT);
}

void SendLeavePacket(int cid) {
	//Do not execute for non logged in user
	if(C[cid].uid == -1) {
		return;
	}
	ev_bye_t ev = {
		.evtype = EV_BYE,
		.cid = cid
	};
	AddEvent( (uint8_t*)&ev, sizeof(ev), SERVER_EVENT);
}

void AcceptNewClient() {
	//Accept new client
	struct sockaddr_in adr;
	socklen_t t = sizeof(adr);
	int client = accept(ServerSocket, (struct sockaddr*)&adr, &t); 
	if(client == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			return; //No connection request
		}
		perror("accept");
		return;
	}
	struct in_addr ip = adr.sin_addr;
	in_port_t prt = ntohs(adr.sin_port);
	//Find empty slot
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd == -1) {
			//init other variables and save client
			memset(&C[i], 0, sizeof(cliinfo_t) );
			C[i].uid = -1;
			C[i].fd = -1;
			C[i].ip = ip;
			C[i].port = prt;
			C[i].fd = client;
			Log(i, "New connection.\n");
			//LogInFile(i, "New connection from %s:%d.\n", inet_ntoa(ip), prt);
			if(fcntl(C[i].fd, F_SETFL, O_NONBLOCK) == -1) {
				DisconnectWithReason(i, "Error.");
				Log(i, "fcntl() failed.\n");
			}
			return;
		}
	}
	//If there's no empty slot, close
	printf("Connection closed, no more empty slots.\n");
	uint8_t tsb[] = " Server is full.";
	tsb[0] = NP_RESP_DISCONNECT;
	send(client, tsb, sizeof(tsb), MSG_NOSIGNAL);
	close(client);
}

void ClientReceive(int cid) {
	uint8_t b[SIZE_NET_BUFFER];
	int r;
	//Read incoming packet
	r = recv(C[cid].fd, b, SIZE_NET_BUFFER, 0);
	if(r == 0) {
		Log(cid, "Client connection closed.\n");
		C[cid].closereq = 1;
		SendLeavePacket(cid);
		return;
	} else if(r == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			return;
		}
		Log(cid, "read() failed: %s. Closing connection.\n", strerror(errno) );
		C[cid].closereq = 1;
		SendLeavePacket(cid);
		return;
	}
	C[cid].timeouttimer = 0;

	//Grantee packet size
	uint8_t tmpbuf[SIZE_NET_BUFFER * 2];
	memcpy(tmpbuf, C[cid].rdbuf, C[cid].rdlength);
	memcpy(&tmpbuf[C[cid].rdlength], b, r);
	size_t p = 0;
	size_t totallen = C[cid].rdlength + r;
	ssize_t rem = totallen;
	while(p < totallen) {
		size_t msgsiz;
		if(C[cid].protocolid == PROTOCOL_RAW || C[cid].protocolid == PROTOCOL_WEBSOCKET) {
			//Websocket or raw
			size_t lenpsiz, lenoff = 0;
			size_t psiz = 0;
			//get entire packet length
			if(C[cid].protocolid == PROTOCOL_WEBSOCKET) {
				lenoff = 1;
				if(rem < 2) {
					break; //at least 2 octets needed to know websocket message len
				}
				size_t t = (tmpbuf[p + 1]) & 0x7f;
				if(t <= 125) {
					lenpsiz = 1;
					psiz = t;
				} else if(t == 126) {
					lenpsiz = 3;
				} else {
					lenpsiz = 9;
				}
			} else {
				if(tmpbuf[p] <= 0xfd) {
					lenpsiz = 1;
					psiz = tmpbuf[p];
				} else if(tmpbuf[p] == 0xfe) {
					lenpsiz = 3;
				} else {
					Log(cid, "ClientReceive(): Bad size identifier 0xff on first byte of packet. Closing connection.\n");
					C[cid].closereq = 1;
					SendLeavePacket(cid);
					return;
				}
			}
			if(rem < lenpsiz + lenoff) {
				break; //more data required to know websocket message len
			}
			//Get message length if message length bytes are not uint8_t
			if(lenpsiz != 1) {
				for(size_t i = 0; i < lenpsiz - 1; i++) {
					psiz += tmpbuf[p + lenoff + i + 1] << ( (lenpsiz - 2 - i) * 8);
				}
			}
			
			//Check packet size
			if(psiz > SIZE_NET_BUFFER) {
				Log(cid, "ClientReceive(): Too long message (%lu).\n", psiz);
				C[cid].closereq = 1;
				SendLeavePacket(cid);
				return;
			}
			size_t hdrsiz = lenpsiz + lenoff;
			if(C[cid].protocolid == PROTOCOL_WEBSOCKET) {
				hdrsiz += 4;
			}
			msgsiz = hdrsiz + psiz;
			if(rem < msgsiz) {
				break; //more data required
			}
			
			//Process packet
			if(C[cid].protocolid == PROTOCOL_WEBSOCKET) {
				int m;
				uint8_t *k;
				if(tmpbuf[p + lenoff] & 0x80) {
					m = 1;
					k = &tmpbuf[p + hdrsiz - 4];
				}
				recv_websock_handler(&tmpbuf[p + hdrsiz], psiz, tmpbuf[p], m, k, cid);
			} else {
				recv_packet_handler(&tmpbuf[p + hdrsiz], psiz, cid);
			}
			msgsiz = hdrsiz + psiz;
		} else {
			//HTTP protocol
			uint8_t* nline = memchr(&tmpbuf[p], '\n', rem);
			if(nline == NULL) {
				break; //If there's no newline, more data required
			}

			//Calculate line size
			msgsiz = nline - &tmpbuf[p] + 1;

			//Limit http message line size to 254
			if(msgsiz >= HTTP_LINE_LIMIT) {
				Log(cid, "HTTP message length overflow.\n");
				C[cid].closereq = 1;
				C[cid].inread = 0;
				return;
			}

			//Pack it into null terminated string
			char httpreq[HTTP_LINE_LIMIT];
			size_t linesiz;
			if(msgsiz >= 2) {
				linesiz = msgsiz - 2;
			} else {
				linesiz = msgsiz - 1;
			}
			memcpy(httpreq, &tmpbuf[p], linesiz);
			httpreq[linesiz] = 0;
			recv_http_handler(httpreq, cid);
			
		}
		p += msgsiz;
		rem -= msgsiz;
		if(C[cid].closereq == 1) { return; }
	}
	//Write back remaining
	if(rem > SIZE_NET_BUFFER) {
		Log(cid, "Buffer overflow condition.\n");
		C[cid].closereq = 1;
		return;
	}
	memcpy(C[cid].rdbuf, &tmpbuf[p], rem);
	C[cid].rdlength = rem;
}

void recv_http_handler(char* reqline, int cid) {
	//Called when HTTP request received
	//Empty line means end of http request
	if(strlen(reqline) == 0) {
		//Switch to native protocol if empty line came first.
		if(strlen(C[cid].httpfirstline) == 0) {
			Log(cid, "Switching to native protocol.\n");
			C[cid].protocolid = PROTOCOL_RAW;
			SendGreetingsPacket(cid);
			
		} else {
			//Complete http request if there's non-empty http message.
			char* httpresponse[SIZE_NET_BUFFER];
			if(memcmp(C[cid].httpfirstline, "GET /", 5) == 0 && strlen(C[cid].httpwskey) != 0 && memcmp(C[cid].httpupgrade, "websocket", 9) == 0) {
				//First line should starts with "GET /" and need field "Sec-WebSocket-Key" and "Upgrade: websocket"
				Log(cid, "HTTP protocol change request received, switching to websocket.\n");
				
				//calculate websocket key
				uint8_t sha1ret[20];
				int outlen;
				EVP_MD_CTX *evp = EVP_MD_CTX_new();
				EVP_DigestInit_ex(evp, EVP_sha1(), NULL);
				EVP_DigestUpdate(evp, C[cid].httpwskey, strlen(C[cid].httpwskey) );
				EVP_DigestUpdate(evp, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 36);
				EVP_DigestFinal_ex(evp, sha1ret, &outlen);
				EVP_MD_CTX_destroy(evp);
				
				//to base64
				char wsskey[HTTP_LINE_LIMIT];
				size_t rpos = 0;
				size_t pb;
				const char LUT[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				for(pb = 0; pb < sizeof(sha1ret); pb++) {
					uint8_t e, f;
					if(pb % 3 == 0) {
						e = (sha1ret[pb] >> 2) & 0x3f;
					} else if(pb % 3 == 1) {
						e = ( ( (sha1ret[pb - 1] & 0x3) << 4) + (sha1ret[pb] >> 4) ) & 0x3f;
					} else if(pb % 3 == 2) {
						e = ( ( (sha1ret[pb - 1] & 0xf) << 2) + (sha1ret[pb] >> 6) ) & 0x3f;
						f = sha1ret[pb] & 0x3f;
					}
					wsskey[rpos] = LUT[e];
					rpos++;
					if(pb % 3 == 2) {
						wsskey[rpos] = LUT[f];
						rpos++;
					}
				}
				if(pb % 3 != 0) {
					if(pb % 3 == 1) {
						wsskey[rpos] = LUT[( (sha1ret[pb - 1] & 0x3) << 4) & 0x3f];
						wsskey[rpos + 1] = '=';
						wsskey[rpos + 2] = '=';
						rpos += 3;
					} else {
						wsskey[rpos] = LUT[( (sha1ret[pb - 1] & 0xf) << 2) & 0x3f];
						wsskey[rpos + 1] = '=';
						rpos += 2;
					}
				}
				wsskey[rpos] = 0;
				
				//Send HTTP response and first websock msg
				char httpresponse[SIZE_NET_BUFFER];
				int r = snprintf(httpresponse, SIZE_NET_BUFFER, "HTTP/1.1 101 Switching Protocol\r\nUpgrade: websocket\r\nConnection: upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", wsskey);
				if(r + 1 > SIZE_NET_BUFFER) {
					Log(cid, "HTTP response buffer overflow.\n");
					C[cid].closereq = 1;
					return;
				}
				send(C[cid].fd, httpresponse, r, MSG_NOSIGNAL);
				C[cid].protocolid = PROTOCOL_WEBSOCKET;
				SendGreetingsPacket(cid);
			} else {
				Log(cid, "Incomplete or non protocol change request.\n");
				C[cid].closereq = 1;
			}
		}
		return;
	}

	//process http message
	if(strlen(C[cid].httpfirstline) == 0) {
		strcpy(C[cid].httpfirstline, reqline); //If first message, copy to httpfirstline
		return;
	}
	
	//Process http header
	//Calculate offset of field name and value
	char *val = strchr(reqline, ':');
	//If there is no letter ':', disconnect because of bad http message
	if(val == NULL) {
		Log(cid, "Bad http message\n");
		C[cid].closereq = 1;
		return;
	}
	
	const char *TARGETHDRS[] = {"sec-websocket-key", "upgrade"};
	char *PDEST[] = {C[cid].httpwskey, C[cid].httpupgrade};
	for(int i = 0; i < 2; i++) {
		//Compare it (case-insensitive)
		const char* L = TARGETHDRS[i];
		const size_t Lsize = strlen(L);
		size_t d = 0;
		size_t f = 0;
		while(L[d] && reqline[d]) {
			if(L[d] == tolower(reqline[d]) ) {
				f++;
			} else {
				break;
			}
			d++;
		}
		if(f != Lsize ) {
			continue;
		}
		
		//copy
		size_t s = 0;
		d = 0;
		while(val[s + 1]) {
			//Ignore space letters before field-value
			if(val[s + 1] != ' ' || d != 0) {
				if(i == 1) {
					PDEST[i][d] = tolower(val[s + 1]); //i = 1 (upgrade field is case-insensitive)
				} else {
					PDEST[i][d] = val[s + 1];
				}
				d++;
			}
			s++;
		}
		PDEST[i][d] = 0; //Null terminate str
		break;
	}
}

void recv_websock_handler(uint8_t *b, size_t blen, uint8_t op, int mask, uint8_t *maskkey, int cid) {
	//Called when websocket packet received.
	uint8_t p[SIZE_NET_BUFFER];
	if( (op & 0x80) == 0) {
		Log(cid, "Websock packet fragmentation is not supported yet.\n");
		DisconnectWithReason(cid, "Bad websocket packet.");
		return;
	}
	if( (op & 0xf) == 0x8) {
		//close packet (0x?8)
		C[cid].closereq = 1;
		Log(cid, "Websocket close packet.\n");
		return;
	} else if( (op & 0xf) == 0x9) {
		//ping packet (0x?9)
		send_websocket_packet("", 0, 0xa, cid);
	}

	//unmask
	if(mask) {
		for(size_t i = 0; i < blen; i++) {
			p[i] = b[i] ^ maskkey[i % 4];
		}
	} else {
		memcpy(p, b, blen);
	}
	recv_packet_handler(p, blen, cid);
}

void recv_packet_handler(uint8_t *b, size_t blen, int cid) {
	//Execute command handler according to command type
	switch(b[0]) {
		case NP_EXCHANGE_EVENTS:
			if(blen - 1 > 0) {
				AddEventCmd(&b[1], blen - 1, (int8_t)cid);
			}
			GetEvent(cid);
		break;
		case NP_LOGIN_WITH_HASH:
			LoginWithHash(&b[1], blen - 1, cid);
		break;
		default:
			Log(cid, "Unknown command (%d): ", blen);
			for(int i = 0; i < blen; i++) {
				printf("%02x ", b[i]);
			}
			printf("\n");
			DisconnectWithReason(cid, "Bad command.");
		break;
	}
}

void SendGreetingsPacket(int cid) {
	np_greeter_t p = {
		.pkttype = NP_GREETINGS,
		.cid = cid
	};
	memcpy(&p.salt, ServerSalt, SALT_LENGTH);
	send_packet(&p, sizeof(np_greeter_t), cid);
}

void AddEventCmd(uint8_t* d, int dlen, int cid) {
	if(!(0 <= cid && cid <= MAX_CLIENTS) ) {
		printf("AddEvent(): Passing bad cid\n");
		return;
	}
	if(!(0 <= C[cid].uid && C[cid].uid <= UserCount) ) {
		Log(cid, "AddEvent: Login first.\n");
		DisconnectWithReason(cid, "Bad command.");
		return;
	}
	if(1 <= dlen && dlen <= LIMIT_ADD_EVENT) {
		AddEvent(d, dlen, cid);
	//	Log(cid, "AddEvent: Event Added, Head:%d\n", EBptr);
	} else {
		Log(cid, "AddEvent: Too long event. (%d)\n", dlen);
		DisconnectWithReason(cid, "Too long event.");
	}
}

void AddEvent(uint8_t* d, int dlen, int cid) {
	//Check entire event size
	if(EBptr + dlen + sizeof(event_hdr_t) >= SIZE_EVENT_BUFFER) {
		Log(cid, "AddEvent: EventBuffer overflow.\n");
		return;
	}
	int evcp = 0;
	int pdlen = 0;
	while(evcp < dlen) {
		//Get event length
		ssize_t evlen;
		uint8_t *evhead = &d[evcp];
		evlen = GetEventPacketSize(evhead, dlen);
		if(evlen == -1) {
			Log(cid, "AddEvent: Bad packet. Disconnecting client.\n");
			if(cid != -1) {
				DisconnectWithReason(cid, "Bad event packet.");
			}
			return;
		}
		
		//Process events
		int copyev = 1;
		if(evhead[0] == EV_CHAT) {
			ev_chat_t *hdr = (ev_chat_t*)&evhead[0];
			size_t chatlen = (size_t)ntohs(hdr->clen);
			int whispercid = hdr->dstcid;
			char chatbuf[NET_CHAT_LIMIT];
			uint8_t* chathead = &evhead[sizeof(ev_chat_t)];
			if(dlen < chatlen || chatlen >= NET_CHAT_LIMIT) {
				Log(cid, "Chat overflow or too short chat packet.\n");
				DisconnectWithReason(cid, "Bad packet.");
				return;
			}
			memcpy(chatbuf, chathead, chatlen);
			chatbuf[chatlen] = 0;
			//is private msg?
			if(whispercid != -1) {
				Log(cid, "Whisper to cid %d\n", whispercid);
			}
			if(chatbuf[0] == '?') {
				Log(cid, "servercommand: %s\n", chatbuf);
				ExecServerCommand(chatbuf, cid);
				copyev = 0; //Do not copy server command chat
			} else {
				Log(cid, "chat: %s\n", chatbuf);
			}
		} else if(evhead[0] == EV_RESET) {
			Log(cid, "AddEvent(): round reset request\n");
			if(GetUserOpLevel(cid) < 1) {
				copyev = 0;
				Log(cid, "AddEvent(): Request denied, bad op level.\n");
			}
		} else if(evhead[0] == EV_CHANGE_ATKGAIN) {
			Log(cid, "AddEvent(): change atkgain\n");
			if(GetUserOpLevel(cid) < 1) {
				copyev = 0;
				Log(cid, "AddEvent(): Request denied, bad op level.\n");
			}
		} else if(evhead[0] == EV_HELLO || evhead[0] == EV_BYE) {
			if(cid != -1) {
				copyev = 0; //EV_HELLO and EV_BYE event can not be issued from client
			}
		}
		if(copyev == 1) {
			memcpy(&EventBuffer[EBptr + sizeof(event_hdr_t) + pdlen], evhead, evlen);
			pdlen += evlen;
		}
		evcp += evlen;
	}

	//Append Event header
	event_hdr_t hdr = {
		.cid = (int8_t)cid,
		.evlen = pdlen
	};
	memcpy(&EventBuffer[EBptr], &hdr, sizeof(hdr) );
	EBptr += sizeof(hdr) + pdlen;
}

void ExecServerCommand(char* cmd, int cid) {
	//Op level 1: reset, 2: ban welcome, 3: adduser deluser 4: stop
	if(strcmp("?stop", cmd) == 0) {
		//stop command
		if(GetUserOpLevel(cid) >= 4) {
			Log(cid, "Server stop request\n");
			ProgramExit = 1;
		} else {
			SendPrivateChat(SERVER_EVENT, cid, "Insufficient permission.");
			Log(cid, "Bad op level\n");
		}

	} else if(memcmp("?adduser ", cmd, 9) == 0) {
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
		}
	
	} else if(memcmp("?ban ", cmd, 5) == 0) {
		//Ban command
		int uid = GetUIDByName(&cmd[5]);
		if(GetUserOpLevel(cid) >= 2) {
			if(uid != -1) {
				Log(cid, "User %s will be banned.\n", UserInformations[uid].usr);
				UserInformations[uid].ban = -1;
				//Disconnect if banned user is connected
				for(int i = 0; i < MAX_CLIENTS; i++) {
					if(C[i].uid == uid && C[i].fd != -1) {
						C[i].closereq = 1;
					}
				}
			} else {
				Log(cid, "Specified user not found.\n");
			}
		} else {
			SendPrivateChat(SERVER_EVENT, cid, "Insufficient permission.");
			Log(cid, "Bad op level.\n");
		}

	} else {
		SendPrivateChat(SERVER_EVENT, cid, "Bad server command");
		Log(cid, "Bad command.\n");
	}
}

int GetUserOpLevel(int cid) {
	int t = C[cid].uid;
	if(0 <= t && t <= UserCount - 1) {
		return UserInformations[t].op;
	}
	return 0;
}

void GetEvent(int cid) {
	char tb[SIZE_EVENT_BUFFER + 1];
	tb[0] = NP_EXCHANGE_EVENTS;
	int elen = EBptr - C[cid].bufcur;
	//Copy events to sending buffer, but get rid of the events that has same owner to sender (except chat)
	uint8_t *head = &EventBuffer[C[cid].bufcur];
	int p = 0;
	int w_ptr = 1;
	while(p < elen) {
		event_hdr_t *evchdr = (event_hdr_t*)&head[p];
		int ecid = evchdr->cid;
		int eclen = evchdr->evlen;
		int chunkp = 0;
		int w_evclen = 0;
		while(chunkp < eclen) {
			uint8_t *evhead = &head[p + sizeof(event_hdr_t) + chunkp];
			int evlen = eclen - chunkp;
			int copyevent = 1;
			int tl = GetEventPacketSize(evhead, evlen);
			if(tl != -1) {
				evlen = tl;
				//Don't loopback except EV_CHAT and EV_RESET
				 if(evhead[0] != EV_CHAT && evhead[0] != EV_RESET) {
					if(ecid == cid) {
						copyevent = 0;
					}
				}
				//Process secret chat (char 0 = destination cid, char1 = 0)
				if(evhead[0] == EV_CHAT) {
					ev_chat_t *hdr = (ev_chat_t*)evhead;
					int dcid = hdr->dstcid;
					if(0 <= dcid && dcid <= MAX_CLIENTS && (cid != dcid && cid != ecid) ) {
						copyevent = 0;
					}
				}
			} else {
				copyevent = 0;
			}
			if(copyevent) {
				memcpy(&tb[w_ptr + sizeof(event_hdr_t) + w_evclen], evhead, evlen);
				w_evclen += evlen;
			}
			chunkp += evlen;
		}
		//Write event chunk header
		if(w_evclen) {
			event_hdr_t w_evchdr = {
				.cid = ecid,
				.evlen = htons(w_evclen)
			};
			memcpy(&tb[w_ptr], &w_evchdr, sizeof(w_evchdr) );
			w_ptr += w_evclen + sizeof(event_hdr_t);
		}
		p += eclen + sizeof(event_hdr_t);
	}
	C[cid].bufcur += elen;
	//Send out data
	send_packet(tb, w_ptr, cid);
	EventBufferGC();
	//Log(cid, "GetEvent()\n");
}

ssize_t send_packet(void *pkt, size_t plen, int cid) {
	uint8_t b[SIZE_NET_BUFFER];
	size_t hdrlen;
	//Add header
	if(C[cid].protocolid == PROTOCOL_RAW) {
		if(plen <= 0xfd) {
			hdrlen = 1;
			b[0] = (uint8_t)plen;
		} else if(0xfe <= plen && plen <= 0xffff) {
			hdrlen = 3;
			uint16_t *t = (uint16_t*)&b[1];
			*t = htons(plen);
		} else {
			Log(cid, "send_packet(): too long packet.\n");
			return -1;
		}
	} else if(C[cid].protocolid == PROTOCOL_WEBSOCKET) {
		return send_websocket_packet(pkt, plen, 2, cid);
	}else {
		Log(cid, "send_packet(): Bad protocol\n");
		return -1;
	}
	if(plen + hdrlen > SIZE_NET_BUFFER) {
		Log(cid, "send_packet(): buffer overflow.\n");
		return -1;
	} 
	memcpy(&b[hdrlen], pkt, plen);
	return send(C[cid].fd, b, plen + hdrlen, MSG_NOSIGNAL);
}

ssize_t send_websocket_packet(void *pkt, size_t plen, uint8_t op, int cid) {
	uint8_t b[SIZE_NET_BUFFER];
	size_t hdrlen;
	b[0] = 0x80 + op;
	if(plen <= 125) {
		hdrlen = 2;
		b[1] = (uint8_t)plen;
	} else if(126 <= plen && plen <= 0xffff) {
		hdrlen = 4;
		b[1] = 126;
		uint16_t *t = (uint16_t*)&b[2];
		*t = htons(plen);
	} else {
		Log(cid, "send_websock_packet(): too long packet.\n");
		return -1;
	}
	if(plen + hdrlen > SIZE_NET_BUFFER) {
		Log(cid, "send_websock_packet(): buffer overflow.\n");
		return -1;
	} 
	memcpy(&b[hdrlen], pkt, plen);
	return send(C[cid].fd, b, plen + hdrlen, MSG_NOSIGNAL);

}

ssize_t GetEventPacketSize(uint8_t *d, int l) {
	ssize_t r = -1;
	if(l < 1) { return -1; } //If data size is not enough to distinguish data type, return -1
	//Get data len according to struct type
	if(d[0] == EV_PLAYABLE_LOCATION) {
		r = sizeof(ev_playablelocation_t);
	} else if (d[0] == EV_PLACE_ITEM) {
		r = sizeof(ev_placeitem_t);
	} else if (d[0] == EV_USE_SKILL) {
		r = sizeof(ev_useskill_t);
	} else if (d[0] == EV_CHAT) {
		ev_chat_t *e = (ev_chat_t*)d;
		int chatlen = ntohs(e->clen);
		if(chatlen < NET_CHAT_LIMIT) {
			r = sizeof(ev_chat_t) + chatlen;
		}
	} else if (d[0] == EV_RESET) {
		r = sizeof(ev_reset_t);
	} else if (d[0] == EV_HELLO) {
		ev_hello_t *e = (ev_hello_t*)d;
		if(e->uname_len < UNAME_SIZE) {
			r = sizeof(ev_hello_t) + e->uname_len;
		}
	} else if (d[0] == EV_BYE) {
		r = sizeof(ev_bye_t);
	} else if(d[0] == EV_CHANGE_PLAYABLE_ID) {
		r = sizeof(ev_changeplayableid_t);
	} else if(d[0] == EV_CHANGE_ATKGAIN) {
		r = sizeof(ev_changeatkgain_t);
	}
	//return -1 if data length is nou enough to store struct
	if(r > l) {
		return -1;
	}
	return r;
}

void EventBufferGC() {
	//Adjust buffer if there's data that read by all clients
	/*
		If there's client 0 and 1,
		+-------------------------------------------------+
	   |                EventBuffer                      |
	   +-------------------------------------------------+
	    <--------->|          |        |
	       ||      |          |      EBptr (Current event head)
		    ||		|			  |
			 ||		|		C[0].bufcur (Client 0 current read pos)
			 ||   C[1].bufcur (Client 1 current read pos)
		 This data can be destroyed since all clients read this data			
	*/
	//Calculate minimum bufcur
	int mincur = EBptr;
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd != -1 && mincur > C[i].bufcur) {
			mincur = C[i].bufcur;
		}
	}
	//Shift actual data
	int slen = EBptr - mincur;
	for(int i = 0; i < slen; i++) {
		EventBuffer[i] = EventBuffer[mincur + i];
	}
	//Shift pointers
	EBptr -= mincur;
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd != -1) {
			C[i].bufcur -= mincur;
		}
	}
}

void LoginWithHash(uint8_t* dat, int dlen, int cid) {
	char* pwdpos;
	if(dlen != SHA512_LENGTH) {
		Log(cid, "LoginWithHash(): Wrong packet length.\n");
		DisconnectWithReason(cid, "Bad command.");
		return;
	}
	if(C[cid].uid != -1) {
	Log(cid, "LoginWithHash(): Already logged in.\n");
		DisconnectWithReason(cid, "Bad command.");
		return;
	}

	//Find matching credentials
	for(int i = 0; i < UserCount; i++) {
		char *uname = UserInformations[i].usr;
		char *password = UserInformations[i].pwd;
		uint8_t loginkey[SHA512_LENGTH];
		size_t loginkeylen = 0;
		//Make password sha512 (username + password + salt)
		EVP_MD_CTX *evp = EVP_MD_CTX_new();
		EVP_DigestInit_ex(evp, EVP_sha512(), NULL);
		if(EVP_DigestUpdate(evp, uname, strlen(uname) ) != 1 ||
			EVP_DigestUpdate(evp, password, strlen(password) ) != 1 ||
			EVP_DigestUpdate(evp, ServerSalt, SALT_LENGTH) != 1) {
			printf("LoginWithHash(): Feeding data to SHA512 generator failed.\n");
			DisconnectWithReason(cid, "Try again.");
			return;
		} else {
			if(EVP_DigestFinal_ex(evp, loginkey, NULL) != 1) {
				printf("LoginWithHash(): SHA512 get digest failed.\n");
				DisconnectWithReason(cid, "Try again.");
				return;
			}
		}
		EVP_MD_CTX_free(evp);
		if(memcmp(loginkey, dat, SHA512_LENGTH ) == 0) {
			//Duplicate login check, BUG: can't re login as same user
			for(int j = 0; j < MAX_CLIENTS; j++) {
				if(C[j].fd != -1 && C[j].uid == i) {
					Log(cid, "User already logged in.\n");
					DisconnectWithReason(cid, "Duplicate login.");
					return;
				}
			}
			Log(cid, "Logged in as user %s(ID=%d)\n", UserInformations[i].usr, i);
			C[cid].uid = i;
			SendUserLists(cid);
			SendJoinPacket(cid);
			return;
		}
	}
	//Close connection if login failed
	Log(cid, "LoginWithPassword: No matching credential.\n");
	DisconnectWithReason(cid, "Login failure.");
}

void SendUserLists(int cid) {
	uint8_t sendbuf[SIZE_NET_BUFFER];
	int w_ptr = 1;
	sendbuf[0] = NP_RESP_LOGON;
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd == -1 || !(0 <= C[i].uid && C[i].uid <= UserCount) ) { continue; }
		char *un = UserInformations[C[i].uid].usr;
		userlist_hdr_t t = {
			.cid = i,
			.uname_len = strlen(un) + 1
		};
		int psize = t.uname_len + sizeof(userlist_hdr_t);
		if(psize + w_ptr >= SIZE_NET_BUFFER) {
			Log(cid, "SendUserLists(): Buffer overflow.\n");
			DisconnectWithReason(cid, "Error.");
			return;
		}
		memcpy(&sendbuf[w_ptr], &t, sizeof(userlist_hdr_t) );
		memcpy(&sendbuf[w_ptr + sizeof(userlist_hdr_t)], un, t.uname_len);
		w_ptr += psize;
	}
	send_packet(sendbuf, w_ptr, cid);
}

void DisconnectWithReason(int cid, char* reason) {
	uint8_t sendbuf[SIZE_NET_BUFFER];
	sendbuf[0] = NP_RESP_DISCONNECT;
	int rlen = strlen(reason);
	if(rlen < SIZE_NET_BUFFER - 1) {
		memcpy(&sendbuf[1], reason, rlen);
	} else {
		Log(cid, "DisconnectWithReason: Buffer overflow.\n");
		memcpy(&sendbuf[1], "OVERFLOW", 8);
		rlen = 8;
	}
	if(C[cid].protocolid != PROTOCOL_HTTP) {
		send_packet(sendbuf, rlen + 1, cid);
	} else {
		Log(cid, "DisconnectWithReason: Disconnected without sending packet, protocol not decided.\n");
	}
	C[cid].closereq = 1;
	SendLeavePacket(cid);
}

