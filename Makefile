CC              = gcc
CFLAGS_NODEBUG  = -Wall -fno-strict-aliasing
CFLAGS          = $(CFLAGS_NODEBUG) -g -DEBUG
CFLAGS_WARNALL  = $(CFLAGS) -Wextra
INCLUDE_DIRS    = -I /usr/include/python2.6/
LDFLAGS         = $(INCLUDE_DIRS) -l ev -I http-parser -l python2.6
PROFILE_CFLAGS  = $(CFLAGS) -pg
OPTFLAGS        = $(CFLAGS_NODEBUG) -O3

FILES           = bjoern.c
FILES_NODEBUG   = $(FILES) http-parser/http_parser.o
FILES_DEBUG     = $(FILES) http-parser/http_parser_debug.o

all:
	$(CC) $(CFLAGS) $(LDFLAGS) -o bjoern $(FILES_DEBUG)

prep:
	$(CC) -E bjoern.c | ${PAGER}

warn-all:
	$(CC) $(CFLAGS_WARNALL) $(LDFLAGS) -o bjoern $(FILES_DEBUG)

profile:
	$(CC) $(PROFILE_CFLAGS) $(LDFLAGS) -o bjoern $(FILES_NODEBUG)

optfull:
	$(CC) $(CFLAGS_NODEBUG) $(OPTFLAGS) $(LDFLAGS) -o bjoern $(FILES_NODEBUG)
	strip bjoern


run: all
	./bjoern

valgrind: all
	valgrind ./bjoern

callgrind: all clean-callgrind
	valgrind --tool=callgrind ./bjoern

clean-callgrind:
	rm -f callgrind*

gprof: profile
	gprof ./bjoern

ab:
	ab -c 100 -n 10000 http://127.0.0.1:8080/
