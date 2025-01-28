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

CC=gcc
PKGCONF=pkg-config cairo pangocairo openssl
CFLAGS=-Wall -Wconversion -Wextra `$(PKGCONF) --cflags` -I/usr/X11R6/include -I/usr/X11R6/include/X11
LDFLAGS=`$(PKGCONF) --libs` -L/usr/X11R6/lib -L/usr/X11R6/lib/X11 -lX11
OBJS=util.o info.o gamesys.o aiproc.o network.o ui.o graphics.o zundagame.o
OUTNAME=zundagame

all: $(OBJS)
	$(CC) $^ -lm $(LDFLAGS) -o $(OUTNAME)

clean:
	rm -f $(OBJS) $(OUTNAME)
