#Zundamon bakage (C) 2024 Kumohakase
#https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
#Please consider supporting me through ko-fi or pateron
#https://ko-fi.com/kumohakase
#https://www.patreon.com/kumohakasemk8
#
#Zundamon bakage powered by Gtk4
#
#Zundamon is from https://zunko.jp/
#(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr
#
#makefile: build script

CC=gcc
CFLAGSEXTRA=`pkg-config --cflags gtk4`
CFLAGS=-g3 -Wconversion $(CFLAGSEXTRA)
LDFLAGS=`pkg-config --libs gtk4`
OBJS=main.o graphics.o util.o info.o gamesys.o aiproc.o network.o osdep.o
OUTNAME=zundagame
RELEASEOUT=game.tar.gz
PKGCTX=*.c main.h readme.md *.png makefile

all: $(OBJS)
	$(CC) $^ -lm $(LDFLAGS) -o $(OUTNAME)

release:
	tar cvf $(RELEASEOUT) $(PKGCTX)

clean:
	rm -f $(OBJS) $(OUTNAME) $(RELEASEOUT)
