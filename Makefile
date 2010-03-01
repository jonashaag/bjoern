CC      = gcc
CFLAGS  = -Wall
LDFLAGS = -l ev

FILES = bjoern

all:
	$(CC) $(CFLAGS) $(LDFLAGS) -o bjoern bjoern.c
