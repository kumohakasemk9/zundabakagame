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

#include "main.h"

extern int32_t ProgramExiting;

uint16_t host2network_fconv_16(int16_t d) {
	return 0;
}

int16_t network2host_fconv_16(uint16_t d) {
	return 0;
}

int32_t network2host_fconv_32(uint32_t d) {
	return 0;
}

uint32_t host2network_fconv_32(int32_t d) {
	return 0;
}

//Calculate linux hash of uname + password + salt string. Max input size is UNAME_SIZE + PASSWD_SIZE + SALT_LENGTH + 1 and store to output, output should pre-allocated 512 bit buffer. returns 0 when success, -1 when fail.
int32_t compute_passhash(char* uname, char* password, uint8_t *salt, uint8_t *output) {
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

//Open and connect tcp socket to hostname and port
int32_t make_tcp_socket(char* hostname, char* port) {
	warn("Not supported in emscripten\n");
	return -1;
}

//Close current connection
int32_t close_tcp_socket() {
	return -1;
}

//Send bytes to connected server
ssize_t send_tcp_socket(uint8_t* ctx, size_t ctxlen) {
	return -1;
}

//Receive bytes from connected server, returns read bytes
ssize_t recv_tcp_socket(uint8_t* ctx, size_t ctxlen) {
	return -1;
}

void detect_syslang() {
	/*
	const char *LOCALEENVS[] = {"LANG", "LANGUAGE", "LC_ALL"};
	for(int i = 0; i < 3; i++) {
		char *t = getenv(LOCALEENVS[i]);
		if(t != NULL && memcmp(t, "ja", 2) == 0) {
			info("Japanese locale detected.\n");
			LangID = LANGID_JP;
			return;
		}
	}*/
	warn("Language auto detect is not supported yet.\n");
}

//get current time (unix epoch) in mS
double get_current_time_ms() {
	return 0;
}

void warn(const char* c, ...) {
}

void info(const char* c, ...) {
}


void fail(const char* c, ...) {
}

void vfail(const char* c, va_list v) {
}
