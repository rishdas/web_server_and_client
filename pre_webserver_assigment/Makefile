CC=gcc
DEPS=web_sever.h
OBJ=web_server_main.o
LIBPATH=./lib
CFLAGS=-I.
LIB=-lwebserver
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $<
build: web_server_main.o
	$(CC) -L$(LIBPATH) $(CFLAGS) -o web_server.bin $(OBJ) $(LIB)
clean:
	rm -f *.o *~ *.bin *.log
