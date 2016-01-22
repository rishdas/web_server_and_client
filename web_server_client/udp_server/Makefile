CC=gcc
OBJ=udp_server.o
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< -g
build: udp_server.o
	$(CC) -o udp_server.bin $(OBJ) -g
clean:
	rm -f *.o *~ *.bin *.log
