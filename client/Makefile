CC=gcc
OBJ=ruft_client.o
DEPS=ruft.h
CFLAGS=-I..
%.o: %.c $(DEPS)
	$(CC) -c -g -o $@ $<
build: ruft_client.o
	$(CC) $(CFLAGS) -o ruft_client.bin $(OBJ) -g
clean:
	rm -f *.o *~ *.bin *.log
