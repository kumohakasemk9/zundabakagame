/*
Zundamon bakage (C) 2025 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

zundagame-gtk3.c: gtk3 entry point, funcs.
*/

#include "inc/zundagame.h"

#include <errno.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gio/gio.h>

extern cairo_surface_t *Gsfc; //game screen
GSocketClient *TCPClient = NULL; //TCP socket connector
GCancellable *TCPClientCanceller = NULL; //TCP socket connector canceller
GSocketConnection *TCPSockConnObj = NULL; //TCP Socket
GSocket *TCPSock = NULL; //TCP Socket (basic)
extern int32_t ProgramExiting;
extern langid_t LangID;
GtkApplication *App; //Gtk session
GtkWidget *ImgW; //Image screen

gboolean mouse_scroll(GtkWidget*, GdkEventScroll*, gpointer);
gboolean mouse_move(GtkWidget*, GdkEventMotion*, gpointer);
gboolean key_event(GtkWidget*, GdkEventKey*, gpointer);
gboolean button_press(GtkWidget*, GdkEventButton*, gpointer);
void activate(GtkApplication*, gpointer);
gboolean gametick_wrapper(gpointer);
gboolean redraw_win(gpointer);
void socket_hwnd(GObject*, GAsyncResult*, gpointer);

int main(int argc, char *argv[]) {
	char *fn = "credentials.txt";
	if(argc > 1) {
		fn = argv[1];
	}

	//Init game
	if(gameinit(fn) == -1) {
		fail("main(): gameinit() failed\n");
		do_finalize();
		return 1;
	}
	info("gameinit(): OK\n");
	
	//Gtk main routine
	int s;
	App = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(App, "activate", G_CALLBACK(activate), NULL);
	s = g_application_run(G_APPLICATION(App), argc, argv);
	g_object_unref(App);
	
	//Finalize
	do_finalize();
	return s;
}

//Called when app run
void activate(GtkApplication *app, gpointer data) {
	//Prepare widget to show image
	ImgW = gtk_image_new_from_surface(Gsfc);

	//Make window
	GtkWidget *win;
	win = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(win), "Zundagame GTK");
	gtk_window_set_default_size(GTK_WINDOW(win), WINDOW_WIDTH, WINDOW_HEIGHT);
	gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
	gtk_container_add(GTK_CONTAINER(win), ImgW);
	gtk_widget_add_events(win, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_SCROLL_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
	gtk_widget_show_all(win);

	//Start timer
	g_timeout_add(10, gametick_wrapper, NULL);
	info("Gametick thread is running now.\n");
	g_timeout_add(30, redraw_win, NULL);

	//Connect event
	g_signal_connect(win, "button-press-event", G_CALLBACK(button_press), NULL);
	g_signal_connect(win, "scroll-event", G_CALLBACK(mouse_scroll), NULL);
	g_signal_connect(win, "motion-notify-event", G_CALLBACK(mouse_move), NULL);
	g_signal_connect(win, "key-press-event", G_CALLBACK(key_event), NULL);
	g_signal_connect(win, "key-release-event", G_CALLBACK(key_event), NULL);
}

//Called every 10mS
gboolean gametick_wrapper(gpointer data) {
	gametick();
	if(ProgramExiting) {
		g_application_quit(G_APPLICATION(App) );
		return FALSE;
	}
	return TRUE;
}

//Mouse scroll handler
gboolean mouse_scroll(GtkWidget *self, GdkEventScroll* evt, gpointer data) {
	//info("Scroll\n");
	static glong t = 0;
	if(t != evt->time) {
		t = evt->time;
	} else {
		return FALSE;
	}
	if(evt->delta_y > 0) {
		mousepressed_handler(MB_UP);
	} else {
		mousepressed_handler(MB_DOWN);
	}
	return FALSE;
}

//Mouse move handler
gboolean mouse_move(GtkWidget *self, GdkEventMotion *evt, gpointer data) {
	//info("Mouse: %lf, %lf\n", evt->x, evt->y);
	mousemotion_handler( (int32_t)evt->x, (int32_t)evt->y);
	return FALSE;
}

//Keyboard release handler
gboolean key_event(GtkWidget *self, GdkEventKey *event, gpointer data) {
	/*if(event->type == GDK_KEY_PRESS) {
		info("press %s\n", event->string);
	} else if(event->type == GDK_KEY_RELEASE) {
		info("release %s\n", event->string);
	}
	
	info("key: %02x\n", event->string[0]);	*/
	char r = (char)event->string[0];

	//translate keyval to original data
	specialkey_t k = SPK_NONE;
	if(r == '\r') {
		k = SPK_ENTER;
	} else if(r == '\b') {
		k = SPK_BS;
	} else if(event->keyval == GDK_KEY_Left) {
		k = SPK_LEFT;
	} else if(event->keyval == GDK_KEY_Right) {
		k = SPK_RIGHT;
	} else if(r == 0x1b) {
		k = SPK_ESC;
	} else if(event->keyval == GDK_KEY_F3) {
		k = SPK_F3;
	}
	if(k == SPK_NONE && !(0x20 <= r && r <= 0x7e) || event->length > 1) {
		return FALSE; //Do not process except ascii and specified controls
	}
		
	//Pass it to wrapper
	if(event->type == GDK_KEY_PRESS) {
		keypress_handler(r, k);
	} else {
		keyrelease_handler(r);
	}
	return FALSE;
}

//Button press handler
gboolean button_press(GtkWidget *self, GdkEventButton *event, gpointer data) {
	static guint32 pt = 0;
	//Supress duplicate event call
	if(pt != event->time) {
		pt = event->time;
	} else {
		return FALSE;
	}
	//translate
	unsigned int b = event->button;
	//info("click: %d\n", b);
	mousebutton_t t = MB_NONE;
	if(b == 1) {
		t = MB_LEFT;
	} else if(b == 3) {
		t = MB_RIGHT;
	}
	mousepressed_handler(t);
	return FALSE;
}

//Called every 30mS
gboolean redraw_win(gpointer data) {
	game_paint();
	//Apply game screen
	gtk_widget_queue_draw(ImgW);
	if(ProgramExiting) {
		g_application_quit(G_APPLICATION(App) );
		return FALSE;
	}
	return TRUE;
}

uint16_t host2network_fconv_16(int16_t d) {
	return g_htons( (uint16_t)d);
}

int16_t network2host_fconv_16(uint16_t d) {
	return (int16_t)g_ntohs(d);
}

int32_t network2host_fconv_32(uint32_t d) {
	return (int32_t)g_htonl(d);
}

uint32_t host2network_fconv_32(int32_t d) {
	return g_ntohl( (uint32_t)d);
}

//Calculate linux hash of uname + password + salt string. Max input size is UNAME_SIZE + PASSWD_SIZE + SALT_LENGTH + 1 and store to output, output should pre-allocated 512 bit buffer. returns 0 when success, -1 when fail.
int32_t compute_passhash(char* uname, char* password, uint8_t *salt, uint8_t *output) {
	GChecksum *cs = g_checksum_new(G_CHECKSUM_SHA512);
	gsize l = SHA512_LENGTH;
	g_checksum_update(cs, (const guchar*)uname, strlen(uname) );
	g_checksum_update(cs, (const guchar*)password, strlen(password) );
	g_checksum_update(cs, (const guchar*)salt, SALT_LENGTH);
	g_checksum_get_digest(cs, (guint8*)output, &l);
	g_checksum_free(cs);

	if(l != SHA512_LENGTH) {
		warn("compute_passhash: Desired length != Actual wrote length.\n");
		return -1;
	}
	return 0;
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
	//If already connected, this function will fail.
	if(TCPClient != NULL || TCPSock != NULL) {
		warn("make_tcp_socket(): already connected.\n");
		return -1;
	}

	//Convert port
	int p = atoi(port);
	if(!is_range(p, 1, 65535) ) {
		warn("make_tcp_socket(): bad port number!");
		return -1;
	}

	//Address resolution and try to connect
	TCPClient = g_socket_client_new();
	TCPClientCanceller = g_cancellable_new();
	g_socket_client_connect_to_host_async(TCPClient, hostname, p, TCPClientCanceller, socket_hwnd, NULL);
	return 0;
}

//socket connect callback
void socket_hwnd(GObject* src, GAsyncResult* ret, gpointer data) {
	info("socket_hwnd(): socket callback\n");
	if(TCPClient == NULL) {
		warn("socket_hwnd(): GSocketClient is freed.\n");
		return;
	}
	GError* err = NULL;
	TCPSockConnObj = g_socket_client_connect_to_host_finish(TCPClient, ret, &err);
	g_object_unref(TCPClient);
	TCPClient = NULL;
	if(err != NULL) {
		warn("socket_hwnd(): %s\n", err->message);
		free(err);
	}
	if(TCPClientCanceller != NULL) {
		g_object_unref(TCPClientCanceller);
		TCPClientCanceller = NULL;
	}
	if(TCPSockConnObj != NULL) {
		TCPSock = g_socket_connection_get_socket(TCPSockConnObj);
		if(TCPSock != NULL) {
			g_socket_set_blocking(TCPSock, FALSE);
		}
	}
}

//Close current connection
int32_t close_tcp_socket() {
	if(TCPClientCanceller != NULL) {
		g_cancellable_cancel(TCPClientCanceller);
		g_object_unref(TCPClientCanceller);
		TCPClientCanceller = NULL;
	}
	if(TCPSockConnObj == NULL && TCPClient == NULL) {
		warn("close_tcp_socket(): socket is not open!\n");
		return -1;
	}
	info("closing remote connection\n");
	if(TCPSockConnObj != NULL) {
		g_io_stream_close(G_IO_STREAM(TCPSockConnObj), NULL, NULL);
		g_object_unref(TCPSockConnObj);
		TCPSock = NULL;
	}
	if(TCPClient != NULL) {
		g_object_unref(TCPClient);
		TCPClient = NULL;
	}
	return 0;
}

//Send bytes to connected server
ssize_t send_tcp_socket(void* ctx, size_t ctxlen) {
	if(TCPSock == NULL) {
		warn("send_tcp_socket(): socket is not open!\n");
		return -1;
	}
	GError *err = NULL;
	ssize_t r = (ssize_t)g_socket_send(TCPSock, (const gchar*)ctx, (gsize)ctxlen, NULL, &err);
	if(r < 0) {
		if(err != NULL) {
			warn("send_tcp_socket(): send failed: %s\n", err->message);
			free(err);
		} else {
			warn("send_tcp_socket(): send failed\n");
		}
		return -1;
	} else if(r != ctxlen) {
		warn("send_tcp_socket(): send failed. incomplete data send.\n");
		return -1;
	}
	return r;
}

//Receive bytes from connected server, returns read bytes
ssize_t recv_tcp_socket(void* ctx, size_t ctxlen) {
	if(TCPSock == NULL) {
		warn("recv_tcp_socket(): socket is not open!\n");
		return -1;
	}
	GError *err = NULL;
	ssize_t r = (ssize_t)g_socket_receive(TCPSock, (gchar*)ctx, (gsize)ctxlen, NULL, &err);
	if(r == 0) {
		return 0;
	} else if(r < 0) {
		if(err != NULL) {
			if(err->code == G_IO_ERROR_WOULD_BLOCK) {
				return -1;
			}
			warn("recv_tcp_socket(): recv() failed: %s\n", err->message);
			free(err);
		} else {
			warn("recv_tcp_socket(): recv() failed\n");
		}
		return -2;
	}
	return r;
}

//Check if socket output is available
int32_t isconnected_tcp_socket() {
	if(TCPSockConnObj != NULL && TCPSock != NULL) {
		return 1;
	}
	return 0;
}

void warn(const char* c, ...) {
	va_list varg;
	va_start(varg, c);
	vfprintf(stderr, c, varg);
	va_end(varg);
}

void info(const char* c, ...) {
	va_list varg;
	va_start(varg, c);
	vprintf(c, varg);
	va_end(varg);
}

void fail(const char* c, ...) {
	va_list varg;
	va_start(varg, c);
	vfail(c, varg);
	va_end(varg);
}

void vfail(const char*c , va_list varg) {
	vfprintf(stderr, c, varg);
}

void detect_syslang() {
	/*const char *LOCALEENVS[] = {"LANG", "LANGUAGE", "LC_ALL"};
	for(int i = 0; i < 3; i++) {
		char *t = getenv(LOCALEENVS[i]);
		if(t != NULL && memcmp(t, "ja", 2) == 0) {
			info("Japanese locale detected.\n");
			LangID = LANGID_JP;
			return;
		}
	}*/
}
