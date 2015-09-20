CC=gcc
OBJ=web_client.o
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< -g
build: web_client.o
	$(CC) -o web_client.bin $(OBJ) -g
clean:
	rm -f *.o *~ *.bin *.log
