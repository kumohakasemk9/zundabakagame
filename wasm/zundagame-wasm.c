/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

main-emscripten.c: wasm functions

*/
#include "../inc/zundagame.h"
#include "sha512/sha512.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

extern void console_put(char*, int); //imported function, print str on console (0: info, 1: warn, 2: err)

extern char CommandBuffer[BUFFER_SIZE];
extern int32_t ProgramExiting;
extern const char* IMGPATHES[IMAGE_COUNT];
extern int32_t CommandCursor;
int32_t ConnectionSocket = -1;
extern langid_t LangID;
extern SMPProfile_t *SMPProfs;
extern int32_t SMPProfCount;
uint8_t WASMRXBuffer[NET_BUFFER_SIZE]; //WASM can not reference Javascript Memory, need to pass data to WASM allocated memory
extern int32_t WebsockMode;
extern smpstatus_t SMPStatus;

void clipboard_readstart_cb() {};

void textinput_on_cb() {};

void textinput_off_cb() {};

char* getPtr_CommandBuffer() {
	return CommandBuffer;
}

int32_t getLimit_CommandBuffer() {
	return BUFFER_SIZE;
}

int32_t get_netbuf_size() {
	return NET_BUFFER_SIZE;
}

int32_t is_websock_mode() {
	return WebsockMode;
}

void set_websock_mode(int32_t v) {
	WebsockMode = v;
}

uint8_t *getPtr_RXBuffer() {
	return WASMRXBuffer;
}

void setSMPProfile(char *hostname, char *port, char *username, char *password) {
	if(SMPProfs == NULL) {
		SMPProfs = malloc(sizeof(SMPProfile_t) );
	}
	strncpy(SMPProfs[0].host, hostname, HOSTNAME_SIZE);
	SMPProfs[0].host[HOSTNAME_SIZE - 1] = 0; //for additional security
	strncpy(SMPProfs[0].port, port, PORTNAME_SIZE);
	SMPProfs[0].port[PORTNAME_SIZE - 1] = 0; //for additional security
	strncpy(SMPProfs[0].usr, username, UNAME_SIZE);
	SMPProfs[0].usr[UNAME_SIZE - 1] = 0; //for additional security
	strncpy(SMPProfs[0].pwd, password, PASSWD_SIZE);
	SMPProfs[0].pwd[PASSWD_SIZE - 1] = 0; //for additional security
	SMPProfCount = 1;
}

int getImageCount() {
	return IMAGE_COUNT;
}

void *getIMGPATHES() {
	return IMGPATHES;
}

int isProgramExiting() {
	return ProgramExiting;
}

int is_cmd_mode() {
	if(CommandCursor == -1) {
		return 0;
	}
	return 1;
}

uint16_t host2network_fconv_16(int16_t d) {
	return htons( (uint16_t)d);
}

int16_t network2host_fconv_16(uint16_t d) {
	return (int16_t)htons(d);
}

int32_t network2host_fconv_32(uint32_t d) {
	return (int32_t)ntohl(d);
}

uint32_t host2network_fconv_32(int32_t d) {
	return htonl( (uint32_t)d);
}

int32_t isconnected_tcp_socket() {
	warn("This function should be never called\n");
	return 0;
}

int32_t compute_passhash(char* username, char* password, uint8_t* salt, uint8_t* output) {
	char in[UNAME_SIZE + PASSWD_SIZE + SALT_LENGTH];
	size_t ul = strlen(username);
	size_t pl = strlen(password);
	size_t l = ul + pl + SALT_LENGTH;
	if(l > sizeof(in) ) {
		warn("compute_passhash(): too long input.\n");
		return -1;
	}
	memcpy(in, username, ul);
	memcpy(&in[ul], password, pl);
	memcpy(&in[ul + pl], salt, SALT_LENGTH);
	crypto_hash_sha512(output, in, l);
	return 0;
}

ssize_t recv_tcp_socket(void* b, size_t l) {
	warn("This function should never be called\n");
	return -1;
}

void detect_syslang() {
	//This won't be executed in WASM ver.
}

void set_language(langid_t l) {
	LangID = l;
}

void warn(const char* c, ...) {
	char buf[BUFFER_SIZE];
	va_list varg;
	va_start(varg, c);
	vsnprintf(buf, BUFFER_SIZE, c, varg);
	va_end(varg);
	console_put(buf, 1);
}

void info(const char* c, ...) {
	char buf[BUFFER_SIZE];
	va_list varg;
	va_start(varg, c);
	vsnprintf(buf, BUFFER_SIZE, c, varg);
	va_end(varg);
	console_put(buf, 0);
}


void fail(const char* c, ...) {
	va_list varg;
	va_start(varg, c);
	vfail(c, varg);
	va_end(varg);
}

void vfail(const char* c, va_list v) {
	char buf[BUFFER_SIZE];
	vsnprintf(buf, BUFFER_SIZE, c, v);
	console_put(buf, 2);
}
