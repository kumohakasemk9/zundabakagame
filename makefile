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
CFLAGSEXTRA=`pkg-config --cflags cairo pangocairo x11 openssl`
CFLAGS=-g3 -Wall -Wconversion -Wextra $(CFLAGSEXTRA)
LDFLAGS=`pkg-config --libs cairo x11 pangocairo openssl`
OBJS=main.o graphics.o util.o info.o gamesys.o aiproc.o network.o ui.o
OUTNAME=zundagame
RELEASEOUT=game.tar.gz
PKGCTX=readme.md img adwaitalegacy $(OUTNAME)

all: $(OBJS)
	$(CC) $^ -lm $(LDFLAGS) -o $(OUTNAME)

release:
	tar cvf $(RELEASEOUT) $(PKGCTX)

clean:
	rm -f $(OBJS) $(OUTNAME) $(RELEASEOUT)
