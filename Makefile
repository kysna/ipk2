CC=g++
CFLAGS= -Wall -pedantic -W

all: client server

client.o: client.cpp
	$(CC) $(CFLAGS) -c client.cpp -o client.o
server.o: server.cpp
	$(CC) $(CFLAGS) -c server.cpp -o server.o

client: client.o
	$(CC) $(CFLAGS) client.o -o client
server: server.o
	$(CC) $(CFLAGS) server.o -o server


