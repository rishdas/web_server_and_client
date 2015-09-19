CC=gcc
OBJ=udp_client.o
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $<
build: udp_client.o
	$(CC) -o udp_client.bin $(OBJ)
clean:
	rm -f *.o *~ *.bin *.log
