CC      		= gcc
CFLAGS  		= -Wall -g -fno-strict-aliasing
LDFLAGS 		= -l ev -pthread
PROFILE_CFLAGS 	= $(CFLAGS) -pg
OPTFLAGS        = $(CFLAGS) -O3

FILES = bjoern

all:
	$(CC) $(CFLAGS) $(LDFLAGS) -o bjoern bjoern.c

profile:
	$(CC) $(PROFILE_CFLAGS) $(LDFLAGS) -o bjoern bjoern.c

optfull:
	$(CC) $(OPTFLAGS) $(LDFLAGS) -o bjoern bjoern.c
