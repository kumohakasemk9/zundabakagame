#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

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

#include <openssl/sha.h>

#define MAX_CLIENTS 20
#define LIMIT_ADD_EVENT 512
#define SIZE_EVENT_BUFFER 8192
#define SIZE_NET_BUFFER 512
#define SHA512_DIGEST_LEN (512 / 8)

#include <zunda-server.h>

//User Information Struct
typedef struct {
	char usr[UNAME_SIZE];
	uint8_t loginkey[SHA512_DIGEST_LEN];
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
} cliinfo_t;

int ServerSocket;
int ProgramExit = 0;
cliinfo_t C[MAX_CLIENTS];
uint8_t EventBuffer[SIZE_EVENT_BUFFER];
int EBptr = 0;
userinfo_t *UserInformations = NULL;
int UserCount = 0;
uint8_t ServerSalt[SALT_LENGTH];

void INTHwnd(int);
void IOHwnd(int);
void NewClient();
int InstallSignal(int, void (*)() );
int MakeAsync(int);
int IsAvailable(int);
void ClientReceive(int);
void AddEvent(uint8_t*, int, int);
void GetEvent(int);
void LoginWithPassword(uint8_t*, int, int);
void DisconnectWithReason(int, char*);
void SendGreetingsPacket(int);
void EventBufferGC();
void Log(int, const char*, ...);

int main(int argc, char *argv[]) {
	//TODO: Timeout mechanism
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
		//Store loginkey in this session SHA512 of (username+password+salt)
		uint8_t rawdat[UNAME_SIZE + PASSWD_SIZE + SALT_LENGTH];
		memcpy(rawdat, b, usernamelen);
		memcpy(&rawdat[usernamelen], &pwdpos[1], passwdlen);
		memcpy(&rawdat[usernamelen+passwdlen], ServerSalt, SALT_LENGTH);
		SHA512(rawdat, usernamelen + passwdlen + SALT_LENGTH, UserInformations[UserCount].loginkey);
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
	if(0 <= cid && cid <= MAX_CLIENTS - 1 && C[cid].fd != -1) {
		printf("<%d", cid);
		if(0 <= C[cid].uid && C[cid].uid <= UserCount) {
			printf("/%s", UserInformations[C[cid].uid].usr);
		}
		printf(">");
	}
	va_list varg;
	va_start(varg, ptn);
	vprintf(ptn, varg);
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
			C[i].fd = client;
			Log(i, "New connection from %s:%d.\n", inet_ntoa(ip), prt);
			if(MakeAsync(client) == -1) {
				DisconnectWithReason(i, "Error.");
				Log(i, "MakeAsync() failed.\n");
			}
			C[i].ip = ip;
			C[i].port = prt;
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
		C[cid].inread = 0;
		return;
	} else if(r == -1) {
		Log(cid, "read() failed: %s. Closing connection.\n", strerror(errno) );
		C[cid].closereq = 1;
		C[cid].inread = 0;
		return;
	}
	//Execute command handler according to command type
	switch(b[0]) {
		case NP_EXCHANGE_EVENTS:
			if(r - 1 > 0) {
				AddEvent(&b[1], r - 1, cid);
			}
			GetEvent(cid);
		break;
		case NP_LOGIN_WITH_PASSWORD:
			LoginWithPassword(&b[1], r - 1, cid);
		break;
		default:
			Log(cid, "Unknown command (%d): ", r);
			for(int i = 0; i < r; i++) {
				printf("%02x ", b[i]);
			}
			printf("\n");
			DisconnectWithReason(cid, "Bad command.");
		break;
	}
	C[cid].inread = 0; //MUTEX
}

void SendGreetingsPacket(int cid) {
	np_greeter_t p = {
		.pkttype = NP_GREETINGS,
		.cid = cid
	};
	memcpy(&p.salt, ServerSalt, SALT_LENGTH);
	send(C[cid].fd, &p, sizeof(np_greeter_t), MSG_NOSIGNAL);
}

void AddEvent(uint8_t* d, int dlen, int cid) {
	event_hdr_t hdr = {
		.cid = cid,
		.evlen = htons(dlen)
	};
	if(C[cid].uid == -1) {
		Log(cid, "AddEvent: Login first.\n");
		DisconnectWithReason(cid, "Bad command.");
		return;
	}
	if(1 <= dlen && dlen <= LIMIT_ADD_EVENT) {
		int addlen = dlen + sizeof(hdr);
		if(EBptr + addlen < SIZE_EVENT_BUFFER) {
			memcpy(&EventBuffer[EBptr], &hdr, sizeof(hdr) );
			memcpy(&EventBuffer[EBptr + sizeof(hdr)], d, dlen);
			EBptr += addlen;
			Log(cid, "AddEvent: Event Added, Head:%d\n", EBptr);
		} else {
			Log(cid, "AddEvent: EventBuffer overflow.\n");
		}
	} else {
		Log(cid, "AddEvent: Too long event. (%d)\n", dlen);
		DisconnectWithReason(cid, "Too long event.");
	}
}

void GetEvent(int cid) {
	char tb[SIZE_EVENT_BUFFER + 1];
	tb[0] = NP_EXCHANGE_EVENTS;
	int elen = EBptr - C[cid].bufcur;
	memcpy(&tb[1], &EventBuffer[C[cid].bufcur], elen);
	C[cid].bufcur += elen;
	//Send out data
	send(C[cid].fd, tb, elen + 1, MSG_NOSIGNAL);
	EventBufferGC();
	Log(cid, "GetEvent()\n");
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
	if(dlen != SHA512_DIGEST_LEN) {
		Log(cid, "LoginWithPassword: Wrong packet length.\n");
		DisconnectWithReason(cid, "Bad command.");
		return;
	}
	if(C[cid].uid != -1) {
		Log(cid, "LoginWithPassword: Already logged in.\n");
		send(C[cid].fd, "p+", 2, MSG_NOSIGNAL);
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
		if(memcmp(UserInformations[i].loginkey, dat, SHA512_DIGEST_LEN ) == 0) {
			Log(cid, "LoginWithPassword: Logged in as user %s(ID=%d)\n", UserInformations[i].usr, i);
			C[cid].uid = i;
			send(C[cid].fd, "p+", 2, MSG_NOSIGNAL);
			return;
		}
	}
	//Close connection if login failed
	Log(cid, "LoginWithPassword: No matching credential.\n");
	DisconnectWithReason(cid, "Login failure.");
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
	send(C[cid].fd, sendbuf, rlen + 1, MSG_NOSIGNAL);
	C[cid].closereq = 1;
}
