/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by Gtk4

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

osdep-windows.c: os dependent codes for windows
*/

#include "main.h"

GSocketConnection *GConn = NULL;
GSocket *GSock;

int32_t make_tcp_socket(char* hostname, char* port) {
	if(GConn != NULL) {
		g_print("make_tcp_socket() win64: already connected.\n");
		return -1;
	}
	int _pn = atoi(port);
	guint16 pn;
	if(1 <= _pn && _pn <= 65535) {
		pn = _pn;
	} else {
		g_print("make_tcp_socket() win64: bad port name\n");
		return -1;
	}
	GSocketClient *gsc = g_socket_client_new();
	GConn = g_socket_client_connect_to_host(gsc, hostname, pn, NULL, NULL);
	g_object_unref(gsc);
	if(GConn == NULL) {
		g_print("make_tcp_socket() win64: Connection failed.\n");
		return -1;
	}
	GSock = g_socket_connection_get_socket(GConn);
	return 0;
}

int32_t close_tcp_socket() {
	if(GConn == NULL) {
		g_print("close_tcp_socket() win64: Connection already closed.\n");
		return -1;
	}
	g_socket_close(GSock, NULL);
	g_object_unref(GConn);
	GConn = NULL;
	return 0;
}

int32_t send_tcp_socket(uint8_t *ctx, size_t ctxlen) {
	if(GConn == NULL) {
		g_print("send_tcp_socket() win64: Connection is not open.\n");
		return -1;
	}
	if(g_socket_send(GSock, ctx, (gsize)ctxlen, NULL, NULL) != (gssize)ctxlen) {
		g_print("send_tcp_socket() win64: send failed. closing connection.\n");
		close_tcp_socket();
		return -1;
	}
	return 0;
}

int32_t install_io_handler() {
	return 0; //In windows, do nothing, windows use poll
}

ssize_t recv_tcp_socket(uint8_t *ctx, size_t ctxlen) {
	if(GConn == NULL) {
		g_print("recv_tcp_socket() win64: Connection is not open.\n");
		return -1;
	}
	return (ssize_t)g_socket_receive(GSock, ctx, (gsize)ctxlen, NULL, NULL);
}

//For windows, use poll strategy
void poll_tcp_socket() {
	if(GConn == NULL) { return; } //If not connected, return
	//If there's any data, call network data receive handler
	if(g_socket_condition_check(GSock, G_IO_IN) == G_IO_IN) {
		net_recv_handler();
	}
}
