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
PKGCTX=*.c main.h readme.md *.png makefile

#For cygwin user
gvsbuild : CFLAGSEXTRA=`gvsbuild/bin/pkg-config --cflags gtk4`
gvsbuild : LDFLAGS=`gvsbuild/bin/pkg-config --libs gtk4`

#For linux-windows crossbuild
cross-gvsbuild : CFLAGSEXTRA=-Igvsbuild/include/gtk-4.0 -Igvsbuild/include/pango-1.0 -Igvsbuild/include/harfbuzz -Igvsbuild/include/gdk-pixbuf-2.0 -Igvsbuild/include/cairo -Igvsbuild/include/glib-2.0 -Igvsbuild/lib/glib-2.0/include -Igvsbuild/include/freetype2 -Igvsbuild/include/graphene-1.0 -Igvsbuild/lib/graphene-1.0/include -mfpmath=sse -msse -msse2 -Igvsbuild/include/libmount -Igvsbuild/include/blkid -Igvsbuild/include/fribidi -Igvsbuild/include/sysprof-6 -pthread -Igvsbuild/include/libpng16 -Igvsbuild/include/pixman-1
cross-gvsbuild : LDFLAGS=-Lgvsbuild/lib -lgtk-4 -lharfbuzz -lpangocairo-1.0 -lpango-1.0 -lgdk_pixbuf-2.0 -lcairo-gobject -lcairo -lgraphene-1.0 -lgio-2.0 -lglib-2.0 -lgobject-2.0 -lws2_32
cross-gvsbuild : CC=x86_64-w64-mingw32-gcc

all: $(OBJS)
	$(CC) $^ -lm $(LDFLAGS) -o $(OUTNAME)

gvsbuild: $(OBJS)
	$(CC) $^ -lm $(LDFLAGS) -o gvsbuild-$(OUTNAME)

cross-gvsbuild: $(OBJS)
	$(CC) $^ -lm $(LDFLAGS) -o gvsbuild-$(OUTNAME)

release-src-w64:
	7z a game.zip $(PKGCTX)

release-src:
	tar cvf game.tar.gz $(PKGCTX)

clean:
	rm -f $(OBJS) $(OUTNAME) gvsbuild-$(OUTNAME).exe
