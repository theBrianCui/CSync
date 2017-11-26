CC = clang-3.5
CFLAGS = -g -std=c99
CLIENT_OBJECTS = client.o sclock.o
SERVER_OBJECTS = server.o sclock.o
.PHONY : clean

client : $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) $(CLIENT_OBJECTS) -o client -lm

server : $(SERVER_OBJECTS)
	$(CC) $(CFLAGS) $(SERVER_OBJECTS) -o server -lm

client.o : client.c sclock.o
	$(CC) $(CFLAGS) -c $<

server.o : server.c sclock.o
	$(CC) $(CFLAGS) -c $<

sclock.o : sclock.c sclock.h
	$(CC) $(CFLAGS) -c $< -lm

clean :
	rm -f time_test client server ./*.o
