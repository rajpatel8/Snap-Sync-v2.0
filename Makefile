# Makefile for Distributed File System Project

# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# List of targets (servers renamed; client remains as w25clients)
TARGETS = server_1 server_2 server_3 server_4 w25clients

all: $(TARGETS)

# Build server_1 from S1.c
server_1: S1.c
	$(CC) $(CFLAGS) -o server_1 S1.c

# Build server_2 from S2.c
server_2: S2.c
	$(CC) $(CFLAGS) -o server_2 S2.c

# Build server_3 from S3.c
server_3: S3.c
	$(CC) $(CFLAGS) -o server_3 S3.c

# Build server_4 from S4.c
server_4: S4.c
	$(CC) $(CFLAGS) -o server_4 S4.c

# Build the client
w25clients: w25clients.c
	$(CC) $(CFLAGS) -o w25clients w25clients.c

clean:
	rm -f $(TARGETS)
