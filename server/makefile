CC=gcc
CFLAGS=-g3
OBJS=server-main.o
OUT=server

$(OUT): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $(OUT)

clear:
	rm -f $(OUT) $(OBJS)
