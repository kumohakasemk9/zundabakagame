/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

main.c: linux entry point, X11 funcs.

TODO List
nickname feature in SMP
chat mute feature in SMP
add lol style death/kill anouncement
add getCurrentPlayableCharacterId()
Make me ahri
change Character constant information structure to each function getters
Delete SMP added character when disconnect from server.
*/

#include "inc/zundagame.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cairo/cairo-xlib.h>
#include <openssl/evp.h>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <poll.h>

#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <errno.h>
#include <string.h>

Display *Disp = NULL; //XDisplay
Window Win; //XWindow
cairo_surface_t *GSsfc; //GameScreen drawer surface
cairo_t *GS; //GameScreen Drawer
extern cairo_surface_t *Gsfc; //game screen
int ConnectionSocket = -1;
extern int32_t ProgramExiting;
extern langid_t LangID;

void redraw_win();
void xwindowevent_handler(XEvent, Atom);
void *thread_cb(void*);

void redraw_win() {
	game_paint();
	//Apply game screen
	cairo_set_source_surface(GS, Gsfc, 0, 0);
	cairo_paint(GS);
}

//X11 window event catcher
void xwindowevent_handler(XEvent ev, Atom wmdel) {
	if(ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wmdel) { //Alt+F4
		ProgramExiting = 1;
	} else if(ev.type == KeyPress || ev.type == KeyRelease) { //Keyboard press or Key Release
		//Get key char andd sym
		char r;
		KeySym ks;
		XLookupString(&ev.xkey, &r, sizeof(r), &ks, NULL);
		
		//translate XKeySym to original data
		specialkey_t k = SPK_NONE;
		if(ks == XK_Return || ks == XK_KP_Enter) {
			k = SPK_ENTER;
		} else if(ks == XK_BackSpace) {
			k = SPK_BS;
		} else if(ks == XK_Left) {
			k = SPK_LEFT;
		} else if(ks == XK_Right) {
			k = SPK_RIGHT;
		} else if(ks == XK_Escape) {
			k = SPK_ESC;
		} else if(ks == XK_F3) {
			k = SPK_F3;
		}
		
		//Pass it to wrapper
		if(ev.type == KeyPress) {
			keypress_handler(r, k);
		} else {
			keyrelease_handler(r);
		}
	} else if(ev.type == MotionNotify) { //Mouse move
		mousemotion_handler( (int32_t)ev.xmotion.x, (int32_t)ev.xmotion.y);
	} else if(ev.type == ButtonPress) { //Mouse button press or wheel
		//translate
		unsigned int b = ev.xbutton.button;
		mousebutton_t t = MB_NONE;
		if(b == Button1) {
			t = MB_LEFT;
		} else if(b == Button3) {
			t = MB_RIGHT;
		} else if(b == Button4) {
			t = MB_UP;
		} else if(b == Button5) {
			t = MB_DOWN;
		}
		
		mousepressed_handler(t);
	}
} 

int main(int argc, char *argv[]) {
	char *fn = "credentials.txt";
	if(argc > 1) {
		fn = argv[1];
	}

	//Set timer
	pthread_t pth1;
	pthread_create(&pth1, NULL, thread_cb, NULL);

	//Init game
	if(gameinit(fn) == -1) {
		fail("main(): gameinit() failed\n");
		do_finalize();
		return 1;
	}
	info("gameinit(): OK\n");
	
	//Connect to X display
	Disp = XOpenDisplay(NULL);
	if(Disp == NULL) {
		fail("main(): Failed to open XDisplay.\n");
		do_finalize();
		return 1;
	}
	info("X11 opened.\n");
	
	//Create and show window
	Window r = DefaultRootWindow(Disp);
	Win = XCreateSimpleWindow(Disp, r, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, 0);
	XStoreName(Disp, Win, "Zundamon game");
	XMapWindow(Disp, Win);
	info("Window opened.\n");

	//Make game screen drawer and its graphic context
	int s = DefaultScreen(Disp);
	Visual *v = DefaultVisual(Disp, s);
	GSsfc = cairo_xlib_surface_create(Disp, Win, v, WINDOW_WIDTH, WINDOW_HEIGHT);
	GS = cairo_create(GSsfc);
	if(cairo_status(GS) != CAIRO_STATUS_SUCCESS) {
		fail("main(): Failed to create game screen drawer.\n");
		cairo_destroy(GS);
		cairo_surface_destroy(GSsfc);
		XDestroyWindow(Disp, Win);
		XCloseDisplay(Disp);
		do_finalize();
	}
	info("Image buffer and Graphics context created.\n");
	
	//Fix window size
	XSizeHints sh = {
		.flags = PMinSize | PMaxSize,
		.min_width = WINDOW_WIDTH,
		.min_height = WINDOW_HEIGHT,
		.max_width = WINDOW_WIDTH,
		.max_height = WINDOW_HEIGHT
	};
	XSetWMNormalHints(Disp, Win, &sh);

	//Select event and message loop
	Atom WM_DELETE_WINDOW = XInternAtom(Disp, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(Disp, Win, &WM_DELETE_WINDOW, 1);
	XSelectInput(Disp, Win, KeyPressMask | KeyReleaseMask | ButtonPressMask | ExposureMask | PointerMotionMask);
	info("Starting message loop.\n");
	double tbefore = get_current_time_ms();
	while(ProgramExiting == 0) {
		XEvent e;
		if(XPending(Disp) ) {
			XNextEvent(Disp, &e);
			xwindowevent_handler(e, WM_DELETE_WINDOW);
		} else {
			//redraw every 30mS
			if(get_current_time_ms() > tbefore + 30) {
				redraw_win();
				tbefore = get_current_time_ms();
			}
			usleep(100);
		}
	}
	
	//Finalize
	do_finalize();
	XDestroyWindow(Disp, Win);
	XCloseDisplay(Disp);
	cairo_destroy(GS);
	cairo_surface_destroy(GSsfc);
	return 0;
}

uint16_t host2network_fconv_16(int16_t d) {
	return htons( (uint16_t)d);
}

int16_t network2host_fconv_16(uint16_t d) {
	return (int16_t)ntohs(d);
}

int32_t network2host_fconv_32(uint32_t d) {
	return (int32_t)htonl(d);
}

uint32_t host2network_fconv_32(int32_t d) {
	return ntohl( (uint32_t)d);
}

//Calculate linux hash of uname + password + salt string. Max input size is UNAME_SIZE + PASSWD_SIZE + SALT_LENGTH + 1 and store to output, output should pre-allocated 512 bit buffer. returns 0 when success, -1 when fail.
int32_t compute_passhash(char* uname, char* password, uint8_t *salt, uint8_t *output) {
	EVP_MD_CTX *evp = EVP_MD_CTX_new();
	EVP_DigestInit_ex(evp, EVP_sha512(), NULL);
	if(EVP_DigestUpdate(evp, uname, strlen(uname) ) != 1 ||
		EVP_DigestUpdate(evp, password, strlen(password) ) != 1 ||
		EVP_DigestUpdate(evp, salt, SALT_LENGTH) != 1) {
		warn("compute_passhash: Feeding data to SHA512 generator failed.\n");
		EVP_MD_CTX_free(evp);
		return -1;	
	}
	unsigned int l = 0;
	if(EVP_DigestFinal_ex(evp, output, &l) != 1) {
		warn("compute_passhash: SHA512 get digest failed.\n");
		EVP_MD_CTX_free(evp);
		return -1;
	}
	if(l != SHA512_LENGTH) {
		warn("compute_passhash: Desired length != Actual wrote length.\n");
		EVP_MD_CTX_free(evp);
		return -1;
	}
	EVP_MD_CTX_free(evp);
	return 0;
}

//Sub thread handler
void *thread_cb(void* p) {
	info("Gametick thread is running now.\n");
	double tbefore = get_current_time_ms();
	while(1) {
		if(get_current_time_ms() > 10 + tbefore) { //10mS Timer
			gametick();
			tbefore = get_current_time_ms();
		}
		usleep(100);
	}
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
	if(ConnectionSocket != -1) {
		warn("make_tcp_socket(): already connected.\n");
		return -1;
	}
	
	//Name Resolution
	struct addrinfo *addr, hint;
	memset(&hint, 0, sizeof(hint) );
	hint.ai_flags = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	int32_t r = getaddrinfo(hostname, port, &hint, &addr);
	if(r != 0) {
		//getaddrinfo error
		warn("make_tcp_socket(): getaddrinfo failed. (%d)\n", r);
		return -1;
	}
	
	//Make socket
	ConnectionSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(ConnectionSocket < 0) {
		//Socket error
		warn("make_tcp_socket(): socket creation failed.\n");
		freeaddrinfo(addr);
		return -1;
	}
	
	int opt = 1;
	setsockopt(ConnectionSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt) );

	//Make socket nonblock.
	if(fcntl(ConnectionSocket, F_SETFL, O_NONBLOCK) == -1) {
		warn("make_tcp_socket(): setting socket option failed.\n");
		close(ConnectionSocket);
		ConnectionSocket = -1;
		freeaddrinfo(addr);
		return -1;
	}
	
	//Connect to node
	if(connect(ConnectionSocket, addr->ai_addr, addr->ai_addrlen) != 0 && errno != EINPROGRESS) {
		warn("make_tcp_socket(): connect failed. %s.\n", strerror(errno) );
		close(ConnectionSocket);
		ConnectionSocket = -1;
		freeaddrinfo(addr);
		return -1;
	}
	freeaddrinfo(addr);
	return 0;
}

//Close current connection
int32_t close_tcp_socket() {
	if(ConnectionSocket == -1) {
		warn("close_tcp_socket(): socket is not open!\n");
		return -1;
	}
	info("closing remote connection\n");
	close(ConnectionSocket);
	ConnectionSocket = -1;
	return 0;
}

//Send bytes to connected server
ssize_t send_tcp_socket(uint8_t* ctx, size_t ctxlen) {
	if(ConnectionSocket == -1) {
		warn("send_tcp_socket(): socket is not open!\n");
		return -1;
	}
	ssize_t r = send(ConnectionSocket, ctx, ctxlen, MSG_NOSIGNAL);
	if(r < 0) {
		warn("send_tcp_socket(): send failed: %s\n", strerror(errno) );
		return -1;
	} else if(r != ctxlen) {
		warn("send_tcp_socket(): send failed. incomplete data send.\n");
		return -1;
	}
	return r;
}

//Receive bytes from connected server, returns read bytes
ssize_t recv_tcp_socket(uint8_t* ctx, size_t ctxlen) {
	if(ConnectionSocket == -1) {
		warn("recv_tcp_socket(): socket is not open!\n");
		return -1;
	}
	ssize_t r = recv(ConnectionSocket, ctx, ctxlen, 0);
	if(r == 0) {
		return 0;
	} else if(r < 0) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			return -1;
		} else {
			warn("recv_tcp_socket(): recv() failed: %s\n", strerror(errno) );
			return -2;
		}
	}
	return r;
}

//Check if socket output is available
int32_t isconnected_tcp_socket() {
	if(ConnectionSocket == -1) {
		warn("isconnected(): socket is not open!\n");
		return 0;
	}
	struct pollfd pfd = {
		.fd = ConnectionSocket,
		.events = POLLOUT
	};
	return poll(&pfd, 1, 0);
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
	const char *LOCALEENVS[] = {"LANG", "LANGUAGE", "LC_ALL"};
	for(int i = 0; i < 3; i++) {
		char *t = getenv(LOCALEENVS[i]);
		if(t != NULL && memcmp(t, "ja", 2) == 0) {
			info("Japanese locale detected.\n");
			LangID = LANGID_JP;
			return;
		}
	}
}
