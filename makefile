CC = gcc
CFLAGS = -g
CLIENT_OBJECTS = client.o sclock.o
SERVER_OBJECTS = server.o sclock.o
.PHONY : clean

client : $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) $(CLIENT_OBJECTS) -o client -lm

server : $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) $(SERVER_OBJECTS) -o server -lm

client.o : client.c sclock.h
	$(CC) $(CFLAGS) -c $<

server.o : server.c sclock.h
	$(CC) $(CFLAGS) -c $<

sclock.o : sclock.c
	$(CC) $(CFLAGS) -c $<

clean :
	rm -f time_test client server ./*.o
