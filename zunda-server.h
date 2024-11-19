/*
	Zundamon bakage (C) 2024 Kumohakase
	https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
	Please consider supporting me through ko-fi or pateron
	https://ko-fi.com/kumohakase
	https://www.patreon.com/kumohakasemk8
 
	Zundamon bakage powered by Gtk4
 
	Zundamon is from https://zunko.jp/
	(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

	zunda-server.h: server client common definitions (like commands)
*/

typedef enum {
	NP_ADD_EVENT = 'e',
	NP_GET_EVENTS = 'g',
	NP_LOGIN_WITH_PASSWORD = 'p',
	NP_RESP_DISCONNECT = 'q'
} server_command_t;

