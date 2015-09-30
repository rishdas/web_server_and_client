CC=gcc
OBJ=ruft_server.o
DEPS=ruft.h
CFLAGS=-I.
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< -g
build: ruft_server.o
	$(CC) $(CFLAGS) -o ruft_server.bin $(OBJ) -g
clean:
	rm -f *.o *~ *.bin *.log
