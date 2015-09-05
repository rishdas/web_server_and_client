CC=gcc
OBJ=web_server.o
CFLAGS=-fpic
LIBFLAGS=-shared
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
build: $(OBJ)
	$(CC) $(LIBFLAGS) -o libwebserver.so $(OBJ)
clean:
	rm -f *.o *~ *.so
