CC      		= gcc
CFLAGS  		= -Wall -g
LDFLAGS 		= -l ev
PROFILE_CFLAGS 	= $(CFLAGS) -pg

FILES = bjoern

all:
	$(CC) $(CFLAGS) $(LDFLAGS) -o bjoern bjoern.c

profile:
	$(CC) $(PROFILE_CFLAGS) $(LDFLAGS) -o bjoern bjoern.c
