#Zundamon bakage (C) 2024 Kumohakase
#https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
#Please consider supporting me through ko-fi or pateron
#https://ko-fi.com/kumohakase
#https://www.patreon.com/kumohakasemk8
#
#Zundamon bakage powered by cairo, X11.
#
#Zundamon is from https://zunko.jp/
#(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr
#
#makefile: build script

CC = gcc
COMCFLAGS = -Wall -Wconversion -Wextra -g3
CFLAGS = $(COMCFLAGS) `pkg-config gtk+-3.0 --cflags`
LDFLAGS = `pkg-config gtk+-3.0 --libs` -lm
COMOBJS = util.o info.o gamesys.o aiproc.o network.o ui.o
OBJS = $(COMOBJS) zundagame-gtk3.o graphics.o
OUTNAME = zundagame-gtk3

ifeq ($(TARGET),X11)
	OBJS = $(COMOBJS) zundagame-x11.o graphics.o
	CFLAGS = $(COMCFLAGS) `pkg-config cairo pangocairo openssl --cflags` -I/usr/X11R6/include -I/usr/X11R6/include/X11
	LDFLAGS = `pkg-config cairo pangocairo openssl --libs` -lX11 -lm -L/usr/X11R6/lib -L/usr/X11R6/lib/X11
	OUTNAME=zundagame-x11
endif

ifeq ($(TARGET),WIN32)
	OBJS=$(COMOBJS) graphics.o zundagame-win32.o
	CFLAGS=$(COMCFLAGS) `$(PKGCFG) cairo pangocairo --cflags`
	LDFLAGS=-lbcrypt -lws2_32 -lm `$(PKGCFG) cairo pangocairo --libs`
	OUTNAME=zundagame-win32
endif

ifeq ($(TARGET),WASM)
	CC=emcc
	OBJS=wasm/zundagame-wasm.o $(COMOBJS)
	CFLAGS=$(COMCFLAGS) -D__WASM
	LDFLAGS=--no-entry -s "EXPORTED_FUNCTIONS=['_gameinit', '_gametick', '_select_next_item', '_select_prev_item', '_use_item', '_switch_character_move', '_game_paint', '_isProgramExiting', '_is_cmd_mode', '_getIMGPATHES', '_getImageCount', '_modifyKeyFlags', '_cmd_putch', '_cmd_enter', '_cmd_cancel', '_cmd_cursor_back', '_cmd_cursor_forward', '_cmd_backspace', '_start_command_mode', '_mousemotion_handler', '_set_language', '_pkt_recv_handler', '_connection_establish_handler', '_connection_close_handler', '_getPtr_RXBuffer', '_switch_locator']" -s ERROR_ON_UNDEFINED_SYMBOLS=0
	OUTNAME=wasm/zundagame.wasm
endif

all: $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $(OUTNAME)

clean:
	rm -f $(OBJS) $(OUTNAME)
