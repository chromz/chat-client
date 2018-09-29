

CC=gcc
CFLAGS=-Wall

objects = chat.o

chat : $(objects)
	$(CC) $(CFLAGS) -o chat $(objects) 

chat.o : src/chat.c
	$(CC) $(CFLAGS) -c src/chat.c

.PHONY : clean
clean :
	-rm chat $(objects)
