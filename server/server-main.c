#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 20
#define LIMIT_ADD_EVENT 512
#define SIZE_EVENT_BUFFER 8192
#define SIZE_NET_BUFFER 512

#include </home/owner/harddisk_home/MyPrograms/zundamon-game/zunda-server.h>

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
	int uid;
} cliinfo_t;

int ServerSocket;
int ProgramExit = 0;
cliinfo_t C[MAX_CLIENTS];
uint8_t EventBuffer[SIZE_EVENT_BUFFER];
int EBptr = 0;
userinfo_t *UserInformations = NULL;
int UserCount = 0;
int ServerSalt;

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

int main(int argc, char *argv[]) {
	//TODO: Timeout mechanism
	ServerSalt = random();
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
		memcpy(UserInformations[UserCount].usr, b, usernamelen);
		UserInformations[UserCount].usr[usernamelen] = 0;
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
	//Server loop
	printf("Server started at *:%d\n", portnum);
	while(ProgramExit == 0) {
		int clients_noreading++; //non connecting clients and non reading clients total
		for(int i = 0; i < MAX_CLIENTS; i++) {
			if(C[i].fd != -1 && C[i].inread == 0) {
				clients_nonreading++;
				//Process close request
				if(C[i].closereq == 1) {
					printf("[%d]: Closing connection.\n", i);
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
			printf("[%d]: new connection from %s:%d.\n", i, inet_ntoa(ip), prt);
			if(MakeAsync(client) == -1) {
				DisconnectWithReason(i, "Error.");
				printf("[%d]: MakeAsync() failed.\n", i);
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
		printf("[%d]: read() failed: %s. Closing connection.\n", cid, strerror(errno) );
		C[cid].closereq = 1;
		C[cid].inread = 0;
		return;
	}
	//Execute command handler according to command type
	switch(b[0]) {
		case NP_ADD_EVENT:
			AddEvent(&b[1], r - 1, cid);
		break;
		case NP_GET_EVENTS:
			GetEvent(cid);
		break;
		case NP_LOGIN_WITH_PASSWORD:
			LoginWithPassword(&b[1], r - 1, cid);
		break;
		//case NP_GREETINGS:
		//	SendGreetingsPacket(cid);
		//break;
		default:
			printf("[%d]: Unknown command (%d): ", cid, r);
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
		.cid = cid,
		.salt = htonl(ServerSalt)
	};
	send(C[cid].fd, &p, sizeof(np_greeter_t), MSG_NOSIGNAL);
}

void AddEvent(uint8_t* d, int dlen, int cid) {
	event_hdr_t hdr = {
		.cid = cid,
		.evlen = htons(dlen)
	};
	if(C[cid].uid == -1) {
		printf("[%d]: AddEvent: Login first.\n", cid);
		DisconnectWithReason(cid, "Bad command.");
		return;
	}
	if(1 <= dlen && dlen <= LIMIT_ADD_EVENT) {
		int addlen = dlen + sizeof(hdr);
		if(EBptr + addlen < SIZE_EVENT_BUFFER) {
			memcpy(&EventBuffer[EBptr], &hdr, sizeof(hdr) );
			memcpy(&EventBuffer[EBptr + sizeof(hdr)], d, dlen);
			EBptr += addlen;
			printf("[%d]: AddEvent: Event Added, Head:%d\n",cid, EBptr);
		} else {
			printf("[%d]: AddEvent: EventBuffer overflow.\n", cid);
		}
	} else {
		printf("[%d]: AddEvent: Too long event.\n", cid);
		DisconnectWithReason(cid, "Too long event.");
	}
}

void GetEvent(int cid) {
	char tb[SIZE_EVENT_BUFFER + 1];
	tb[0] = NP_GET_EVENTS;
	int elen = EBptr - C[cid].bufcur;
	memcpy(&tb[1], &EventBuffer[C[cid].bufcur], elen);
	C[cid].bufcur += elen;
	//Send out data
	send(C[cid].fd, tb, elen + 1, MSG_NOSIGNAL);
	EventBufferGC();
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
	if(C[cid].uid != -1) {
		printf("[%d]: LoginWithPassword: Already logged in.\n", cid);
		send(C[cid].fd, "p+", 2, MSG_NOSIGNAL);
		return;
	}
	pwdpos = memchr(dat, ':', dlen);
	if(pwdpos == NULL) {
		//packet format error
		printf("[%d]: LoginWithPassword: Malformed packet!!\n", cid);
		DisconnectWithReason(cid, "Bad command.");
		return;
	}
	//Parameter length check
	int usrlen = (uint8_t*)pwdpos - dat;
	int pwdlen = dlen - usrlen - 1;
	if(usrlen >= UNAME_SIZE || pwdlen >= PASSWD_SIZE) {
		printf("[%d]: LoginWithPassword: Too long credential!!\n", cid);
		DisconnectWithReason(cid, "Bad command.");
		return;
	}
	//Find matching credentials
	for(int i = 0; i < UserCount; i++) {
		char* user = UserInformations[i].usr;
		char* pass = UserInformations[i].pwd;
		if(memcmp(user, dat, strlen(user) ) == 0 && memcmp(pass, &pwdpos[1], strlen(pass) ) == 0 ) {
			printf("[%d]: LoginWithPassword: Logged in as user %s(ID=%d)\n", cid, UserInformations[i].usr, i);
			C[cid].uid = i;
			send(C[cid].fd, "p+", 2, MSG_NOSIGNAL);
			return;
		}
	}
	//Close connection if login failed
	printf("[%d]: LoginWithPassword: No matching credential.\n", cid);
	DisconnectWithReason(cid, "Login failure.");
}

void DisconnectWithReason(int cid, char* reason) {
	uint8_t sendbuf[SIZE_NET_BUFFER];
	sendbuf[0] = NP_RESP_DISCONNECT;
	int rlen = strlen(reason);
	if(rlen < SIZE_NET_BUFFER - 1) {
		memcpy(&sendbuf[1], reason, rlen);
	} else {
		printf("[%d]: DisconnectWithReason: Buffer overflow.\n");
		memcpy(&sendbuf[1], "OVERFLOW", 8);
		rlen = 8;
	}
	send(C[cid].fd, sendbuf, rlen + 1, MSG_NOSIGNAL);
	C[cid].closereq = 1;
}
