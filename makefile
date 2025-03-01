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
COMCFLAGS = -Wall -Wconversion -Wextra
CFLAGS = $(COMCFLAGS) `pkg-config cairo pangocairo openssl --cflags` -I/usr/X11R6/include -I/usr/X11R6/include/X11 -g3
LDFLAGS = `pkg-config cairo pangocairo openssl --libs` -lm -lX11 -lm -L/usr/X11R6/lib -L/usr/X11R6/lib/X11
COMOBJS = util.o info.o gamesys.o aiproc.o network.o ui.o
OBJS = $(COMOBJS) zundagame.o graphics.o
OUTNAME = zundagame

ifeq ($(TARGET),WASM)
	CC=emcc
	OBJS=wasm/zundagame-wasm.o wasm/sha512/sha512.o $(COMOBJS)
	CFLAGS=$(COMCFLAGS) -D__WASM
	LDFLAGS=--no-entry -s "EXPORTED_FUNCTIONS=['_getLimit_CommandBuffer', '_getPtr_CommandBuffer', '_execcmd', '_gameinit', '_gametick', '_game_paint', '_isProgramExiting', '_is_cmd_mode', '_getIMGPATHES', '_getImageCount', '_mousemotion_handler', '_keypress_handler', '_keyrelease_handler', '_set_language', '_net_message_handler', '_process_tcp_packet', '_connection_establish_handler', '_connection_close_handler', '_getPtr_RXBuffer', '_is_websock_mode', '_get_netbuf_size', '_set_websock_mode']" -s ERROR_ON_UNDEFINED_SYMBOLS=0
	OUTNAME=wasm/zundagame.wasm
endif

all: $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $(OUTNAME)

clean:
	rm -f $(OBJS) $(OUTNAME)
