/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

server.c: SMP server main code
*/

//fix bug: segfault in EventBufferGC() when connecting with chrome ws
//fix bug: event buffer overflow (make multiple EV_PLAYER_LOCATION event to one)

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>

#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include <openssl/evp.h>

#include "../inc/server.h"

#define PROTOCOL_HTTP 0
#define PROTOCOL_WEBSOCKET 1
#define PROTOCOL_RAW 2

int ServerSocket;
int ProgramExit = 0;
cliinfo_t C[MAX_CLIENTS];
uint8_t EventBuffer[SIZE_EVENT_BUFFER];
int EBptr = 0;
userinfo_t *UserInformations = NULL;
int UserCount = 0;
uint8_t ServerSalt[SALT_LENGTH];
FILE *LogFile;
int Timeout = 100; //0 to disable timeout

void INTHwnd(int);
void AcceptNewClient();
void ClientReceive(int);
void LoginWithHash(uint8_t*, int, int);
void SendGreetingsPacket(int);
void SendUserLists(int);
void recv_packet_handler(uint8_t*, size_t, int);
void recv_http_handler(char*, int);
void recv_websock_handler(uint8_t*, size_t, uint8_t, int, uint8_t*, int);
ssize_t send_websocket_packet(void*, size_t, uint8_t, int);
void AcceptUserLogin(int, int);
int64_t get_time_ms();

int main(int argc, char *argv[]) {
	int portnum = 25566;
	char *passwdfile = "server_passwd";
	//Process parameters
	for(int i = 1; i < argc; i++) {
		if(strcmp(argv[i], "-p") == 0) {
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
		} else if(strcmp(argv[i], "-f") == 0) {
			if(i + 1 < argc) {
				passwdfile = argv[i + 1];
			} else {
				printf("You need to specify password file!!\n");
				return 1;
			}
		} else if(strcmp(argv[i], "-t") == 0) {
			if(i + 1 < argc) {
				int t = atoi(argv[i + 1]);
				if(0 <= t) {
					Timeout = t;
				} else {
					printf("Bad timeout.\n");
				}
			} else {
				printf("You need to specify timeout.\n");
				return 1;
			}

		}
	}

	//Init variables
	for(int i = 0; i < SALT_LENGTH; i++) {
		ServerSalt[i] = random() % 0x100;
	}
	for(int i = 0; i < MAX_CLIENTS; i++) {
		C[i].fd = -1;
	}

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
					if(C[i].timeouttimer < get_time_ms() ) {
						Log(i, "Timed out.\n");
						DisconnectWithReason(i, "Server timeout.");
					}
				}
			}
		}
		EventBufferGC();
		usleep(1);
	}
	printf("Closing server.\n");
	
	//Write user informations back
	f_pwdfile = fopen(passwdfile, "w");
	if(f_pwdfile == NULL) {
		printf("Can not write to %s: %s\n", passwdfile, strerror(errno) );
	} else {
		for(int i = 0; i < UserCount; i++) {
			if(UserInformations[i].usr[0] == 0) {
				continue;
			}
			fprintf(f_pwdfile, "%s:%s:%s:%ld:%d:\n", UserInformations[i].usr, UserInformations[i].pwd, UserInformations[i].banreason, UserInformations[i].ban, UserInformations[i].op);
		}
	}
	
	//Finish
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

int64_t get_time_ms() {
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return (t.tv_sec * 1000) + (t.tv_nsec / 1000000);
}

int AddUser(char* line) {
	char *offs[5];
	size_t lens[5];
	const size_t SIZELIMS[] = {UNAME_SIZE, PASSWD_SIZE, BAN_REASON_SIZE, 32, 2};
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
		CopyAsString(outs[i], offs[i], lens[i]);
	}
	//Store op, ban
	UserInformations[aid].op = 0;
	UserInformations[aid].ban = 0;
	for(int i = 0; i < 2; i++) {
		char t[32];
		CopyAsString(t, offs[i + 3], lens[i + 3]);
		if(i == 0) {
			UserInformations[aid].ban = atol(t);
		} else {
			UserInformations[aid].op = atoi(t);
		}
	}
	printf("User add: %s, op=%d, banexpire=%d\n", UserInformations[aid].usr, UserInformations[aid].op, UserInformations[aid].ban);
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
	if(cid == -1) {
		printf("[server]");
		fprintf(LogFile, "[server]");
	}
	va_list varg;
	va_start(varg, ptn);
	vprintf(ptn, varg);
	va_end(varg);
	va_start(varg, ptn);
	vfprintf(LogFile, ptn, varg);
	va_end(varg);
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
			C[i].timeouttimer = get_time_ms() + Timeout;
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
	C[cid].timeouttimer = get_time_ms() + Timeout;

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
			AcceptUserLogin(cid, i);
			return;
		}
	}
	//Close connection if login failed
	Log(cid, "LoginWithPassword: No matching credential.\n");
	DisconnectWithReason(cid, "Login failure.");
}

void AcceptUserLogin(int cid, int uid) {
	userinfo_t t = UserInformations[uid];
	//Duplicate login check
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd != -1 && C[i].uid == uid) {
			Log(cid, "User already logged in.\n");
			DisconnectWithReason(cid, "Duplicate login.");
			return;
		}
	}
	//Ban check
	if(t.banreason[0] != 0 && (t.ban > time(NULL) || t.ban == 0) ) {
		time_t rem = t.ban - time(NULL);
		char st[SIZE_NET_BUFFER - 1];
		if(t.ban == 0) {
			Log(cid, "User %s(%d) is banned for %s.\n", t.usr, uid, t.banreason);
			snprintf(st, SIZE_NET_BUFFER - 2, "BANNED: %s", t.banreason);
		} else {
			Log(cid, "User %s(%d) is banned for %s (expeires in %d sec).\n", t.usr, uid, t.banreason, rem);
			snprintf(st, SIZE_NET_BUFFER - 2, "SUSPENDED: %s (%d)", t.banreason, rem);
		}
		st[SIZE_NET_BUFFER - 2] = 0;
		DisconnectWithReason(cid, st);
		return;
	}
	Log(cid, "Logged in as user %s(%d)\n", t.usr, uid);
	C[cid].uid = uid;
	SendUserLists(cid);
	SendJoinPacket(cid);
}

void SendUserLists(int cid) {
	uint8_t sendbuf[SIZE_NET_BUFFER];
	int w_ptr = 1;
	sendbuf[0] = NP_RESP_LOGON;
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(C[i].fd == -1 || !(0 <= C[i].uid && C[i].uid <= UserCount) ) { continue; }
		char *un = UserInformations[C[i].uid].usr;
		size_t uname_len = strnlen(un, UNAME_SIZE - 1);
		int psize = uname_len + 2;
		if(psize + w_ptr >= SIZE_NET_BUFFER) {
			Log(cid, "SendUserLists(): Buffer overflow.\n");
			DisconnectWithReason(cid, "Error.");
			return;
		}
		sendbuf[w_ptr] = i;
		CopyAsString(&sendbuf[w_ptr + 1], un, uname_len);
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

int GetUIDByName(char* n) {
	for(int i = 0; i < UserCount; i++) {
		char *t = UserInformations[i].usr;
		if(memcmp(t, n, strlen(t) ) == 0) {
			return i;
		}
	}
	return -1;
}

int GetUserOpLevel(int cid) {
	int t = C[cid].uid;
	if(0 <= t && t <= UserCount - 1) {
		return UserInformations[t].op;
	}
	return 0;
}

void CopyAsString(char* dst, char *src, size_t l) {
	memcpy(dst, src, l);
	dst[l] = 0;
}

