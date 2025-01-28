#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>

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

#include <inc/zunda-server.h>

//User Information Struct
typedef struct {
	char usr[UNAME_SIZE];
	char pwd[PASSWD_SIZE];
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

void SendJoinPacket(int);
void SendLeavePacket(int);
void INTHwnd(int);
void IOHwnd(int);
void NewClient();
int InstallSignal(int, void (*)() );
int MakeAsync(int);
int IsAvailable(int);
void ClientReceive(int);
void AddEventCmd(uint8_t*, int, int);
void AddEvent(uint8_t*, int, int);
void GetEvent(int);
void LoginWithPassword(uint8_t*, int, int);
void DisconnectWithReason(int, char*);
void SendGreetingsPacket(int);
void EventBufferGC();
void Log(int, const char*, ...);
int GetEventPacketSize(uint8_t*, int);
void SendUserLists(int);
void recv_packet_handler(uint8_t*, size_t, int);
ssize_t send_packet(void*, size_t, int);

int main(int argc, char *argv[]) {
	//Generate salt for user credential cipher
	for(int i = 0; i < SALT_LENGTH; i++) {
		ServerSalt[i] = random() % 0x100;
	}
	for(int i = 0; i < MAX_CLIENTS; i++) { C[i].fd = -1; }
	//Install signal
	if(InstallSignal(SIGINT, INTHwnd) == -1 || InstallSignal(SIGIO, IOHwnd) == -1) {
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
	if(MakeAsync(ServerSocket) == -1) {
		printf("Making socket async failed.\n");
		close(ServerSocket);
		return 1;
	}
	//Bind
	int portnum = 25566;
	if(argc >= 2) {
		int t = atoi(argv[1]);
		if(1 <= t && t <= 65535) {
			portnum = t;
		}
	}
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
	FILE *f_pwdfile = fopen("server_passwd", "r");
	if(f_pwdfile == NULL) {
		printf("Can not load server_passwd.\n");
		return 1;
	}
	while(1) {
		char b[8192];
		if(fgets(b, sizeof(b) - 1, f_pwdfile) == NULL) {
			break;
		}
		b[8191] = 0; //for additional security
		if(strlen(b) == 0) { continue; }
		char *pwdpos = strchr(b, ':');
		char *endpos = strchr(b, '\n');
		if(pwdpos == NULL || endpos == NULL) {
			printf("server_passwd: format error.\n");
			fclose(f_pwdfile);
			free(UserInformations);
			return 1;
		}
		int usernamelen = pwdpos - b;
		if(usernamelen >= UNAME_SIZE) {
			printf("server_passwd: Too long username.\n");
			fclose(f_pwdfile);
			free(UserInformations);
			return 1;
		}
		int passwdlen = endpos - pwdpos - 1;
		if(passwdlen >= PASSWD_SIZE) {
			printf("server_passwd: Too long password.\n");
			fclose(f_pwdfile);
			free(UserInformations);
			return 1;
		}
		//Append data to list
		if(UserCount == 0) {
			//First loop
			UserInformations = malloc(sizeof(userinfo_t) );
		} else {
			UserInformations = realloc(UserInformations, sizeof(userinfo_t) * (UserCount + 1) );
		}
		//Store username
		memcpy(UserInformations[UserCount].usr, b, usernamelen);
		UserInformations[UserCount].usr[usernamelen] = 0;
		//Store password
		memcpy(UserInformations[UserCount].pwd, &pwdpos[1], passwdlen);
		UserInformations[UserCount].pwd[passwdlen] = 0;
		UserCount++;
	}
	fclose(f_pwdfile);
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
		int clients_noreading; //non connecting clients and non reading clients total
		for(int i = 0; i < MAX_CLIENTS; i++) {
			if(C[i].fd != -1 && C[i].inread == 0) {
				clients_noreading++;
				//Process close request
				if(C[i].closereq == 1) {
					Log(i, "Closing connection.\n");
					close(C[i].fd);
					C[i].fd = -1;
				}
				//Process timeout
				if(C[i].timeouttimer + 30 < time(NULL) ) {
					Log(i, "Timeout.\n");
					close(C[i].fd);
					C[i].fd = -1;
				}
			} else if(C[i].fd == -1) {
				clients_noreading++;
			}
			//If there's no reading clients (other thread not running), do GC
			if(clients_noreading == MAX_CLIENTS) {
				EventBufferGC();
			}
		}
		usleep(1);
	}
	//Finish
	printf("Closing server.\n");
	fclose(LogFile);
	close(ServerSocket);
	free(UserInformations);
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd != -1) { close(C[i].fd); }
	}
	return 0;
}

void INTHwnd(int) {
	ProgramExit = 1;
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
	vfprintf(LogFile, ptn, varg);
	va_end(varg);
}

int InstallSignal(int s, void (*sh)() ) {
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa) );
	sa.sa_handler = sh;
	return sigaction(s, &sa, NULL);
}

void IOHwnd(int) {
	if(IsAvailable(ServerSocket) ) {
		NewClient();
	}
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd != -1 && IsAvailable(C[i].fd) ) {
			ClientReceive(i);
		}
	}
}

int MakeAsync(int fd) {
	if(fcntl(fd, F_SETFL, O_ASYNC) == -1 || fcntl(fd, F_SETOWN, getpid() ) == -1) {
		return -1;
	}
	return 0;
}

int IsAvailable(int fd) {
	struct pollfd pfd = {
		.fd = fd,
		.events = POLLIN
	};
	return poll(&pfd, 1, 0);
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
	ev_bye_t ev = {
		.evtype = EV_BYE,
		.cid = cid
	};
	AddEvent( (uint8_t*)&ev, sizeof(ev), SERVER_EVENT);
}

void NewClient() {
	//Accept new client
	struct sockaddr_in adr;
	socklen_t t = sizeof(adr);
	int client = accept(ServerSocket, (struct sockaddr*)&adr, &t); 
	if(client == -1) {
		perror("accept");
		return;
	}
	struct in_addr ip = adr.sin_addr;
	in_port_t prt = ntohs(adr.sin_port);
	//Find empty slot
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd == -1) {
			//init other variables and save client
			C[i].uid = -1;
			C[i].closereq = 0;
			C[i].inread = 0;
			C[i].rdlength = 0;
			C[i].timeouttimer = time(NULL);
			C[i].ip = ip;
			C[i].port = prt;
			C[i].fd = client;
			Log(i, "New connection.\n");
			//LogInFile(i, "New connection from %s:%d.\n", inet_ntoa(ip), prt);
			if(MakeAsync(client) == -1) {
				DisconnectWithReason(i, "Error.");
				Log(i, "MakeAsync() failed.\n");
			}
			SendGreetingsPacket(i);
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
	C[cid].inread = 1; //MUTEX
	//Read incoming packet
	r = recv(C[cid].fd, b, SIZE_NET_BUFFER, 0);
	if(r == 0) {
		C[cid].closereq = 1;
		SendLeavePacket(cid);
		C[cid].inread = 0;
		return;
	} else if(r == -1) {
		Log(cid, "read() failed: %s. Closing connection.\n", strerror(errno) );
		C[cid].closereq = 1;
		SendLeavePacket(cid);
		C[cid].inread = 0;
		return;
	}
	//Grantee packet size
	uint8_t tmpbuf[SIZE_NET_BUFFER * 2];
	memcpy(tmpbuf, C[cid].rdbuf, C[cid].rdlength);
	memcpy(&tmpbuf[C[cid].rdlength], b, r);
	size_t p = 0;
	size_t totallen = C[cid].rdlength + r;
	while(p < totallen) {
		size_t lensiz;
		size_t reqsiz;
		size_t rem = totallen - p;
		//get packet size
		if(tmpbuf[p] <= 0xfd) {
			lensiz = 1;
			reqsiz = tmpbuf[p];
		} else if(tmpbuf[p] == 0xfe) {
			lensiz = 3;
			if(rem < lensiz) {
				break; //more data required
			}
			uint16_t *t = (uint16_t*)&tmpbuf[p + 1];
			reqsiz = (size_t)ntohs(*t);
		} else if(tmpbuf[p] == 0xff) {
			Log(cid, "packet decoding: Bad size identifier 0xff on first byte of packet. Closing connection.\n");
			C[cid].closereq = 1;
			SendLeavePacket(cid);
			C[cid].inread = 0;
			return;
		}
		if(rem < lensiz + reqsiz) {
			break; //more data required
		}
		//Process packet
		/*
		Log(cid, "Incoming Packet (%d): ", reqsiz);
		for(int i = 0; i < reqsiz; i++) {
			printf("%02x ", tmpbuf[p + i + lensiz]);
		}
		printf("\n");
		*/
		recv_packet_handler(&tmpbuf[p + lensiz], reqsiz, cid);
		p += lensiz + reqsiz;
	}
	//Write back remaining
	size_t rem = totallen - p;
	memcpy(C[cid].rdbuf, &tmpbuf[p], rem);
	C[cid].rdlength = rem;
	C[cid].inread = 0; //MUTEX
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
		case NP_LOGIN_WITH_PASSWORD:
			LoginWithPassword(&b[1], blen - 1, cid);
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
	int evcp = 0;
	event_hdr_t hdr = {
		.cid = (int8_t)cid,
		.evlen = dlen
	};
	int addlen = dlen + sizeof(hdr);
	if(EBptr + addlen >= SIZE_EVENT_BUFFER) {
		Log(cid, "AddEvent: EventBuffer overflow.\n");
		return;
	}
	while(evcp < dlen) {
		int evlen;
		uint8_t *evhead = &d[evcp];
		evlen = GetEventPacketSize(evhead, dlen);
		if(dlen == -1) {
			Log(cid, "AddEvent: Bad packet. Disconnecting client.\n");
			if(cid != -1) {
				DisconnectWithReason(cid, "Bad event packet.");
			}
			return;
		}
		if(evhead[0] == EV_CHAT) {
			int chatlen = evlen - sizeof(ev_chat_t);
			char chatbuf[NET_CHAT_LIMIT];
			memcpy(chatbuf, evhead + sizeof(ev_chat_t), chatlen);
			chatbuf[chatlen] = 0;
			Log(cid, "chat: %s\n", chatbuf);
		} else if(evhead[0] == EV_RESET) {
			Log(cid, "AddEvent(): round reset request\n");
		}
		evcp += dlen;
	}
	memcpy(&EventBuffer[EBptr], &hdr, sizeof(hdr) );
	memcpy(&EventBuffer[EBptr + sizeof(hdr)], d, dlen);
	EBptr += addlen;
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
				//Ignore EV_HELLO and EV_BYE events if their event owner is not server.
				if(evhead[0] == EV_BYE || evhead[0] == EV_HELLO) {
					if(ecid != SERVER_EVENT) {
						copyevent = 0;
					}
				//EV_RESET should be issued by super user (uid0)
				} else if(evhead[0] == EV_RESET) {
					if(0 <= ecid && ecid <= MAX_CLIENTS && C[ecid].uid != 0) {
						copyevent = 0;
					}
				//For other events, don't loopback except EV_CHAT
				} else if(evhead[0] != EV_CHAT) {
					if(ecid == cid) {
						copyevent = 0;
					}
				}
			} else {
				copyevent = 1;
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
	size_t sizlen;
	if(plen <= 0xfd) {
		sizlen = 1;
		b[0] = (uint8_t)plen;
	} else if(0xfe <= plen && plen <= 0xffff) {
		sizlen = 3;
		uint16_t *t = (uint16_t*)&b[1];
		*t = htons(plen);
	} else {
		Log(cid, "send_packet(): too long packet.\n");
		return -1;
	}
	if(plen + sizlen > SIZE_NET_BUFFER) {
		Log(cid, "send_packet(): buffer overflow.\n");
		return -1;
	}
	memcpy(&b[sizlen], pkt, plen);
	return send(C[cid].fd, b, plen + sizlen, MSG_NOSIGNAL);
}

int GetEventPacketSize(uint8_t *d, int l) {
	int r = -1;
	if(l < 1) { return -1; } //If data size is not enough to distinguish data type, return -1
	//Get data len according to struct type
	if(d[0] == EV_CHANGE_PLAYABLE_SPEED) {
		r = sizeof(ev_changeplayablespeed_t);
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

void LoginWithPassword(uint8_t* dat, int dlen, int cid) {
	char* pwdpos;
	if(dlen != SHA512_LENGTH) {
		Log(cid, "LoginWithPassword: Wrong packet length.\n");
		DisconnectWithReason(cid, "Bad command.");
		return;
	}
	if(C[cid].uid != -1) {
		Log(cid, "LoginWithPassword: Already logged in.\n");
		DisconnectWithReason(cid, "Bad command.");
		return;
	}
/*
	Log(cid, "Attemped to log in with key: ");
	for(int i = 0; i < dlen; i++) {
		printf("%02x ", dat[i]);
	}
	printf("\n");
*/
	//Find matching credentials
	for(int i = 0; i < UserCount; i++) {
		//Make password hash
		char *uname = UserInformations[i].usr;
		char *password = UserInformations[i].pwd;
		uint8_t loginkey[SHA512_LENGTH];
		EVP_MD_CTX *evp = EVP_MD_CTX_new();
		EVP_DigestInit_ex(evp, EVP_sha512(), NULL);
		if(EVP_DigestUpdate(evp, uname, strlen(uname) ) != 1 ||
			EVP_DigestUpdate(evp, password, strlen(password) ) != 1 ||
			EVP_DigestUpdate(evp, ServerSalt, SALT_LENGTH) != 1) {
			printf("LoginWithPassword: Feeding data to SHA512 generator failed.\n");
		} else {
			if(EVP_DigestFinal_ex(evp, loginkey, NULL) != 1) {
				printf("LoginWithPassword: SHA512 get digest failed.\n");
			}
		}
		EVP_MD_CTX_free(evp);
		
		if(memcmp(loginkey, dat, SHA512_LENGTH ) == 0) {
			Log(cid, "LoginWithPassword: Logged in as user %s(ID=%d)\n", UserInformations[i].usr, i);
			//Duplicate login check, BUG: can't re login as same user
			for(int j = 0; j < MAX_CLIENTS; j++) {
				if(C[j].fd != -1 && C[j].uid == i) {
					Log(cid, "LoginWithPassword: User already logged in.\n");
					DisconnectWithReason(cid, "Duplicate login.");
					return;
				}
			}
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
	sendbuf[0] = NP_LOGIN_WITH_PASSWORD;
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
	send_packet(sendbuf, rlen + 1, cid);
	C[cid].closereq = 1;
	SendLeavePacket(cid);
}
