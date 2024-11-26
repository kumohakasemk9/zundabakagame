CC=gcc
CFLAGS=-g3 -I.. `pkg-config --cflags openssl`
OBJS=server-main.o
OUT=server

$(OUT): $(OBJS)
	$(CC) `pkg-config --libs openssl` $^ -o $(OUT)

clear:
	rm -f $(OUT) $(OBJS)
