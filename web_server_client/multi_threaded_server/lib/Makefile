CC=gcc
OBJ=web_server.o
CFLAGS=-fpic -g
LIBFLAGS=-shared
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
build: $(OBJ)
	$(CC) $(LIBFLAGS) -o libwebserver.so $(OBJ) -g
clean:
	rm -f *.o *~ *.so
