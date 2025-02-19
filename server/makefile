CC=gcc
CFLAGS=-g3 -I.. `pkg-config --cflags openssl`
OBJS=server.o commands.o events.o
OUT=server

$(OUT): $(OBJS)
	$(CC) `pkg-config --libs openssl` $^ -o $(OUT)

clean:
	rm -f $(OUT) $(OBJS)
