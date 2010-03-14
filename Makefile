CC      		= gcc
CFLAGS  		= -Wall -g -fno-strict-aliasing
CFLAGS_NODEBUG  = -Wall -fno-strict-aliasing
LDFLAGS 		= -l ev
PROFILE_CFLAGS 	= $(CFLAGS) -pg
OPTFLAGS        = $(CFLAGS_NODEBUG) -O3

FILES = bjoern.c

all:
	$(CC) $(CFLAGS) $(LDFLAGS) -o bjoern $(FILES)

profile:
	$(CC) $(PROFILE_CFLAGS) $(LDFLAGS) -o bjoern $(FILES)

optfull:
	$(CC) $(OPTFLAGS) $(LDFLAGS) -o bjoern $(FILES)
	strip bjoern

run: all
	./bjoern

prep:
	$(CC) -E bjoern.c
