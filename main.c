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
avoid blocking main thread by connect()
nickname feature in SMP
chat mute feature in SMP
change difficulty algorithm (from increasing enemy to increase hp)
add lol style death/kill anouncement
add getCurrentPlayableCharacterId()
Make me ahri
Make it multiplay - Integrate with kumo auth system
Port it to allegro
change Character constant information structure to each function getters
*/

#include "main.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo/cairo-xlib.h>
#include <unistd.h>
#include <sys/time.h>

Display *Disp = NULL; //XDisplay
Window Win; //XWindow
cairo_surface_t *GSsfc; //GameScreen drawer surface
cairo_t *GS; //GameScreen Drawer

void sigalrm_handler(int);
void sigio_handler(int);
int install_handler(int, void (*)(int) );
void xwindowevent_handler(XEvent, Atom);
extern int32_t ProgramExiting;

//Gametick timer, called every 10 mS
void sigalrm_handler(int) {
	gametick();
}

//SIGIO handler
void sigio_handler(int) {
	net_recv_handler();
}

//X11 window event catcher
void xwindowevent_handler(XEvent ev, Atom wmdel) {
	if(ev.type == ClientMessage && ev.xclient.data.l[0] == wmdel) { //Alt+F4
		ProgramExiting = 1;
	} else if(ev.type == Expose) { //Redraw
		game_paint();
		XClearArea(Disp, Win, 0, 0, 1, 1, TRUE); //request repaint
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
		} else if(k == XK_Right) {
			k = SPK_RIGHT;
		} else if(k == XK_Escape) {
			k = SPK_ESC;
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
			t == MB_UP;
		} else if(b == Button5) {
			t = MB_DOWN;
		}
		
		mousepressed_handler(t);
	}
} 

//useful sigaction wrapper
int install_handler(int sign, void (*hwnd)(int) ) {
	struct sigaction s;
	memset(&s, 0, sizeof(struct sigaction) ); //init
	s.sa_handler = hwnd;
	return sigaction(sign, &s, NULL);
}

int main(int argc, char *argv[]) {
	//Install network IO handler
	if(install_handler(SIGIO, sigio_handler) != 0) {
		printf("main(): Could not prepare for network play!\n");
		return 1;
	}

	//Install timer handler and timer
	if(install_handler(SIGALRM, sigalrm_handler) != 0) {
		printf("main(): Could not prepare gametick timer!\n");
		return 1;
	}
	struct itimerval tval = {
		.it_interval.tv_sec = 0,
		.it_interval.tv_usec = 10000,
		.it_value.tv_sec = 0,
		.it_value.tv_usec = 10000
	};
	if(setitimer(ITIMER_REAL, &tval, NULL) != 0) {
		printf("main(): Could not set interval of gametick timer!\n");
		return 1;
	}

	//Init game
	if(gameinit() == -1) {
		printf("main(): activate(): gameinit() failed\n");
		do_finalize();
		return 1;
	}
	
	//Connect to X display
	Disp = XOpenDisplay(NULL);
	if(Disp == NULL) {
		printf("main(): Failed to open XDisplay.\n");
		do_finalize();
		return 1;
	}
	
	//Create and show window
	Window r = DefaultRootWindow(Disp);
	Win = XCreateSimpleWindow(Disp, r, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, 0);
	XMapWindow(Disp, Win);

	//Make game screen drawer and its graphic context
	int s = DefaultScreen(Disp);
	Visual *v = DefaultVisual(Disp, s);
	GSsfc = cairo_xlib_surface_create(Disp, Win, v, WINDOW_WIDTH, WINDOW_HEIGHT);
	GS = cairo_create(GSsfc);
	if(cairo_status(GS) != CAIRO_STATUS_SUCCESS) {
		printf("main(): Failed to create game screen drawer.\n");
		XDestroyWindow(Disp, Win);
		XCloseDisplay(Disp);
		do_finalize();
	}
	
	//Select event and message loop
	Atom WM_DELETE_WINDOW = XInternAtom(Disp, "WM_DELETE_WINDOW", FALSE);
	XSetWMProtocols(Disp, Win, &WM_DELETE_WINDOW, 1);
	XSelectInput(Disp, Win, KeyPressMask | KeyReleaseMask | ButtonPressMask | ExposureMask | PointerMotionMask);
	while(ProgramExiting == 0) {
		XEvent e;
		XNextEvent(Disp, &e);
		xwindowevent_handler(e, WM_DELETE_WINDOW);
	}
	
	do_finalize();
	XDestroyWindow(Disp, Win);
	XCloseDisplay(Disp);
	
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
