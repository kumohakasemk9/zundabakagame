/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8
Zundamon bakage powered by cairo, X11.
Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr
windows-zunda-main.c: windows entry point, windows apis, socket wrapper.
*/
#include "inc/zundagame.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#include <cairo/cairo-win32.h>
specialkey_t SPKFlag;
SOCKET Sock = INVALID_SOCKET;
extern langid_t LangID;
extern cairo_surface_t *Gsfc;
BCRYPT_ALG_HANDLE AlgHwnd;
PBYTE HashObj;
int32_t HashObjSize;
LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
void paint_window(HWND);
void mousemove_wrapper(LPARAM);
void mousebutton_wrapper(int);
void mousewheel_wrapper(WPARAM);
void keychar_handler(WPARAM);
void keydown_handler(WPARAM);
void keyup_handler(WPARAM);
void CALLBACK timer1_cb(PVOID, BOOLEAN);
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hpinst, char* cmdline, int cmdshow) {	
	//Prepare hash object
	int32_t hashretsize, t;
	if(BCryptOpenAlgorithmProvider(&AlgHwnd, BCRYPT_SHA512_ALGORITHM, NULL, 0) != 0) {
		warn("WinMain() : Can not generate hash provider.\n");
		return 1;
	}
	//Check hash return size, it should be 64 for sha512
	BCryptGetProperty(AlgHwnd, BCRYPT_HASH_LENGTH, (PBYTE)&hashretsize, sizeof(DWORD), &t, 0);
	if(hashretsize != SHA512_LENGTH) {
		warn("WinMain(): Hash return size != %d\n", SHA512_LENGTH);
		BCryptCloseAlgorithmProvider(AlgHwnd, 0);
		return 1;
	}
	
	//Init game
	if(gameinit("credentials.txt") == -1) {
		warn("WinMain(): gameinit() failed\n");
		free(HashObj);
		BCryptCloseAlgorithmProvider(AlgHwnd, 0);
		do_finalize();
		return 1;
	}
	info("gameinit(): OK\n");
	// Register the window class.
	const char CLASS_NAME[]  = "Sample Window Class";
	WNDCLASS wc;
	memset(&wc, 0, sizeof(WNDCLASS) );
	wc.lpfnWndProc = window_proc;
	wc.hInstance = hinst;
	wc.lpszClassName = CLASS_NAME;
	RegisterClass(&wc);
	// Create the window.
	HWND h = CreateWindowA(CLASS_NAME, "ZUNDAGAME", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH + 20, WINDOW_HEIGHT + 50, NULL, NULL, hinst, NULL);
	if (h == NULL) {
		warn("WinMain(): CreateWindowA() errored. Window creation fail.\n");
		free(HashObj);
		BCryptCloseAlgorithmProvider(AlgHwnd, 0);
		do_finalize();
		return 1;
	}
	ShowWindow(h, cmdshow);
	//Install gametick timer
	HANDLE th;
	if(CreateTimerQueueTimer(&th, NULL, timer1_cb, NULL, 10, 10, WT_EXECUTEDEFAULT) == 0) {
		printf("WinMain(): Creating timer failed.\n");
		free(HashObj);
		BCryptCloseAlgorithmProvider(AlgHwnd, 0);
		do_finalize();
		return 1;
	}
	// Run the message loop.
	MSG m;
	while(GetMessage(&m, NULL, 0, 0) > 0) {
		TranslateMessage(&m);
		DispatchMessage(&m);
	}
	
	//Finalize
	free(HashObj);
	BCryptCloseAlgorithmProvider(AlgHwnd, 0);
	DeleteTimerQueueTimer(NULL, th, NULL);
	do_finalize();
	return 0;
}
LRESULT CALLBACK window_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		paint_window(h);
		return 0;
	case WM_MOUSEMOVE:
		mousemove_wrapper(lp);
		return 0;
	case WM_LBUTTONDOWN:
		mousebutton_wrapper(0);
		return 0;
	case WM_RBUTTONDOWN:
		mousebutton_wrapper(1);
		return 0;
	case WM_MOUSEWHEEL:
		mousewheel_wrapper(wp);
		return 0;
	case WM_CHAR:
		keychar_handler(wp);
		return 0;
	case WM_KEYDOWN:
		keydown_handler(wp);
		return 0;
	case WM_KEYUP:
		keyup_handler(wp);
		return 0;
	}
	return DefWindowProc(h, msg, wp, lp);
}
void timer1_cb(PVOID lpParameter, BOOLEAN TimeOrWaitFired) {
	static int mutex = 0;
	if(mutex) {return;}
	mutex = 1;
	gametick();
	mutex = 0;
}
void paint_window(HWND h) {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(h, &ps);
	// All painting occurs here, between BeginPaint and EndPaint.
	//Make cairo objects
	cairo_surface_t *sfc = cairo_win32_surface_create(hdc);
	cairo_t *cr = NULL;
	if(sfc != NULL) { cr = cairo_create(sfc); }
	
	//draw
	if(cr != NULL) {
		game_paint();
		cairo_set_source_surface(cr, Gsfc, 0, 0);
		cairo_paint(cr);
	}
	//Finalize
	if(cr != NULL) { cairo_destroy(cr); }
	if(sfc != NULL) { cairo_surface_destroy(sfc); }
	EndPaint(h, &ps);
	InvalidateRect(h, &ps.rcPaint, FALSE);
}
void mousemove_wrapper(LPARAM lp) {
	mousemotion_handler( (int32_t)GET_X_LPARAM(lp), (int32_t)GET_Y_LPARAM(lp) );
}
void mousebutton_wrapper(int mb) {
	mousebutton_t t;
	//printf("MOUSEBUTTON: %d\n", mb);
	if(mb == 0) {
		t = MB_LEFT;
	} else if(mb == 1) {
		t = MB_RIGHT;
	}
	mousepressed_handler(t);
}
void mousewheel_wrapper(WPARAM wp) {
	mousebutton_t t;
	int a = GET_WHEEL_DELTA_WPARAM(wp);
	//printf("MOUSEWHEEL: %d\n", a);
	if(a > 0) {
		t = MB_UP;
	} else {
		t = MB_DOWN;
	}
	mousepressed_handler(t);
}
void keychar_handler(WPARAM wp) {
	int32_t k = (int)wp;
	keypress_handler(k, SPKFlag);
}
void keydown_handler(WPARAM wp) {
	if(wp == VK_LEFT) {
		SPKFlag = SPK_LEFT;
		keypress_handler(0, SPK_LEFT);
	} else if(wp == VK_RIGHT) {
		SPKFlag = SPK_RIGHT;
		keypress_handler(0, SPK_RIGHT);
	} else if(wp == VK_ESCAPE) {
		SPKFlag = SPK_ESC;
	} else if(wp == VK_RETURN) {
		SPKFlag = SPK_ENTER;
	} else if(wp == VK_BACK) {
		SPKFlag = SPK_BS;
	} else if(wp == VK_F3) {
		SPKFlag = SPK_F3;
		keypress_handler(0, SPK_F3);
	} else {
		SPKFlag = SPK_NONE;
	}
	//printf("KEYDOWN: %d\n", wp);
}
void keyup_handler(WPARAM wp) {
	//printf("KEYUP: %d\n", wp);
	if(is_range(wp, 0x41, 0x5a) ) {
		keyrelease_handler(wp); //A to Z
	} 
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
	//Create hash object
	BCRYPT_HASH_HANDLE hashhwnd;
	BCryptCreateHash(AlgHwnd, &hashhwnd, HashObj, HashObjSize, NULL, 0, 0);
	
	//Feed data
	if(BCryptHashData(hashhwnd, uname, strlen(uname), 0) != 0) {
		warn("compute_passhash(): can not feed uname\n");
		BCryptDestroyHash(hashhwnd);
		return -1;
	}
	if(BCryptHashData(hashhwnd, password, strlen(password), 0) != 0) {
		warn("compute_passhash(): can not hash password\n");
		BCryptDestroyHash(hashhwnd);
		return -1;
	}
	if(BCryptHashData(hashhwnd, salt, SALT_LENGTH, 0) != 0) {
		warn("compute_passhash(): can not hash password\n");
		BCryptDestroyHash(hashhwnd);
		return -1;
	}
	
	//extract sha512
	if(BCryptFinishHash(hashhwnd, output, SHA512_LENGTH, 0) != 0) {
		warn("compute_passhash(): can not compute hash\n");
		BCryptDestroyHash(hashhwnd);
		return -1;
	}
	BCryptDestroyHash(hashhwnd);
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
	if(Sock != INVALID_SOCKET) {
		warn("make_tcp_socket(): Already connected.\n");
		return -1;
	}
	
	//Init Winsock
	WSADATA wsadat;
	if(WSAStartup(MAKEWORD(2, 2), &wsadat) != 0) {
		warn("make_tcp_socket(): winsock init failed.\n");
		return -1;
	}
	
	//Make socket
	Sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(Sock == INVALID_SOCKET) {
		warn("make_tcp_socket(): can not create socket.\n");
		WSACleanup();
		return -1;
	}
	
	//Connect
	const TIMEVAL to = {
		.tv_sec = 2,
		.tv_usec = 0
	};
	if(WSAConnectByNameA(Sock, hostname, port, 0, NULL, 0, NULL, &to, NULL) == FALSE) {
		warn("make_tcp_socket(): can not connect to host.\n");
		closesocket(Sock);
		WSACleanup();
		Sock = INVALID_SOCKET;
		return -1;
	}
	return 0;
}
//Close current connection
int32_t close_tcp_socket() {
	if(Sock == INVALID_SOCKET) {
		warn("close_tcp_socket(): socket is not open!\n");
		return -1;
	}
	closesocket(Sock);
	Sock = INVALID_SOCKET;
	return 0;
}
//Send bytes to connected server
ssize_t send_tcp_socket(uint8_t* ctx, size_t ctxlen) {
	if(Sock == INVALID_SOCKET) {
		warn("send_tcp_socket(): socket is not open!\n");
		return -1;
	}
	ssize_t r = send(Sock, ctx, ctxlen, 0);
	if(r != ctxlen) {
		warn("send_tcp_socket(): send failed. Closing current connection.\n");
		//close_tcp_socket();
		return -1;
	}
	return r;
}
//Receive bytes from connected server, returns read bytes, -1 if no data, -2 if error
ssize_t recv_tcp_socket(uint8_t* ctx, size_t ctxlen) {
	if(Sock == INVALID_SOCKET) {
		warn("recv_tcp_socket(): socket is not open!\n");
		return -1;
	}
	
	//Check if there's data
	struct pollfd pfd = {
		.fd = Sock,
		.events = POLLRDNORM
	};
	if(WSAPoll(&pfd, 1, 0) != 1) {
		return -1; //No data
	}
	
	//Recv
	ssize_t r = recv(Sock, ctx, ctxlen, 0);
	if(r < 0) {
		return -2; //error
	}
	return r;
}
int32_t isconnected_tcp_socket() {
	return 1; //This will return 1 always, using blocking connect() in windows version
}
void detect_syslang() {
	LANGID l = GetUserDefaultUILanguage();
	info("Detected locale: %04x\n", l);
	if( (l & 0xff) == 0x11) {
		info("日本語が検出されました\n");
		LangID = LANGID_JP;
	}
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
