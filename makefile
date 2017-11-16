CC = gcc
CFLAGS = -g
CLIENT_OBJECTS = client.o sclock.o
SERVER_OBJECTS = server.o sclock.o

client : $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) $(CLIENT_OBJECTS) -o client

server : $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) $(SERVER_OBJECTS) -o server

%.o : %.c
	$(CC) $(CFLAGS) -c $<
