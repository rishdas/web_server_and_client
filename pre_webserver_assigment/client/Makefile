CC=gcc
OBJ=web_client.o
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $<
build: web_client.o
	$(CC) -o web_client.bin $(OBJ)
clean:
	rm -f *.o *~ *.bin *.log
