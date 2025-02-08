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
#include <stdarg.h>
#include <string.h>

#include <arpa/inet.h>

extern void console_put(char*, int); //imported function, print str on console (0: info, 1: warn, 2: err)

extern int32_t ProgramExiting;
extern const char* IMGPATHES[IMAGE_COUNT];
extern int32_t CommandCursor;
int32_t ConnectionSocket = -1;
extern langid_t LangID;
extern SMPProfile_t *SMPProfs;
extern int32_t SMPProfCount;
uint8_t WASMRXBuffer[NET_BUFFER_SIZE]; //WASM can not reference Javascript Memory, need to pass data to WASM allocated memory

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

ssize_t recv_tcp_socket(uint8_t* b, size_t a) {
	warn("This function should be never called\n");
	return 0;
}

int32_t isconnected_tcp_socket() {
	warn("This function should be never called\n");
	return 0;
}

int32_t compute_passhash(char* username, char* password, uint8_t* salt, uint8_t* output) {
	warn("compute_passhash(): Not supported in wasm. We have no openssl, and JavaScript crypto module is not friendly for wasm import. :(\n");
	return -1;
}

/*
void clipboard_read_handler(GObject* obj, GAsyncResult* res, gpointer data) {
	//Data type check
	const GdkContentFormats* f = gdk_clipboard_get_formats(GClipBoard);
	gsize i;
	const GType* t = gdk_content_formats_get_gtypes(f, &i);
	if(t == NULL) {
		g_print("main.c: clipboard_read_handler(): gdk_content_formats_get_gtypes() failed.\n");
		return;
	}
	if(i != 1 || t[0] != G_TYPE_STRING) {
		g_print("main.c: clipboard_read_handler(): Data type missmatch.\n");
		return;
	}
	//Get text and insert into CommandBuffer
	char *cb = gdk_clipboard_read_text_finish(GClipBoard, res, NULL);
	//g_print("Clipboard string size: %d\nCommandBuffer length: %d\n", l, (uint32_t)strlen(CommandBuffer));
	CommandBufferMutex = TRUE;
	if(utf8_insertstring(CommandBuffer, cb, CommandCursor, sizeof(CommandBuffer) ) == 0) {
		CommandCursor += (int32_t)g_utf8_strlen(cb, 65535);
	} else {
		g_print("main.c: clipboard_read_handler(): insert failed.\n");
	}
	CommandBufferMutex = FALSE;
	free(cb);
}

*/

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
