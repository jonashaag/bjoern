FEATURES    	= -D WANT_ROUTING -D WANT_CACHING

CC              = gcc
CFLAGS_NODEBUG  = -std=c99 -pedantic -Wall -fno-strict-aliasing -shared $(FEATURES)
CFLAGS_NODEBUGP = $(CFLAGS_NODEBUG) -g
CFLAGS          = $(CFLAGS_NODEBUGP) -D DEBUG
CFLAGS_WARNALL  = $(CFLAGS) -Wextra
INCLUDE_DIRS    = -I .							\
				  -I src                     	\
				  -I src/headers 				\
				  -I /usr/include/python2.6/	\
				  -I include 					\
				  -I include/http-parser
LDFLAGS         = $(INCLUDE_DIRS) -l ev -I http-parser -l python2.6
CFLAGS_OPTDEBUG = $(CFLAGS_NODEBUGP) -O3
CFLAGS_OPT      = $(CFLAGS_NODEBUG) -O3
CLFAGS_OPTSMALL = $(CFLAGS_NODEBUG) -Os

OUTFILES		= _bjoern.so
FILES           = src/bjoern.c
FILES_NODEBUG   = $(FILES) include/http-parser/http_parser.o
FILES_DEBUG     = $(FILES) include/http-parser/http_parser_debug.o

TEST            = python tests
PAGER		 	= less


all: clean
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(OUTFILES) $(FILES_DEBUG)

nodebugprints:
	$(CC) $(CFLAGS_NODEBUGP) $(LDFLAGS) -o $(OUTFILES) $(FILES_DEBUG)

prep:
	$(CC) $(LDFLAGS) $(CFLAGS) -E $(FILES) | ${PAGER}

assembler:
	$(CC) $(LDFLAGS) $(CFLAGS_NODEBUGP) -S $(FILES)

assembleropt:
	$(CC) $(LDFLAGS) $(CFLAGS_OPTDEBUG) -S $(FILES)

warnall:
	$(CC) $(CFLAGS_WARNALL) $(LDFLAGS) -o $(OUTFILES) $(FILES_DEBUG)

opt:
	$(CC) $(CFLAGS_OPT) $(LDFLAGS) -o $(OUTFILES) $(FILES_NODEBUG)
	strip $(OUTFILES)

optdebug:
	$(CC) $(CFLAGS_OPTDEBUG) $(LDFLAGS) -o $(OUTFILES) $(FILES_DEBUG)
	strip $(OUTFILES)

optsmall:
	$(CC) $(CFLAGS_OPTSMALL) $(LDFLAGS) -o $(OUTFILES) $(FILES_NODEBUG)

clean:
	rm -f *.o
	rm -f *.pyc


run: nodebugprints
	$(TEST)

runwithdebug: all
	$(TEST)

gdb: all
	gdb python

cgdb: all
	cgdb python

valgrind: nodebugprints
	valgrind $(TEST)

memcheck: nodebugprints
	valgrind --tool=memcheck --leak-check=full --show-reachable=yes $(TEST)

callgrind: nodebugprints clean-callgrind
	valgrind --tool=callgrind $(TEST)

callgrind-opt: optdebug clean-callgrind
	valgrind --tool=callgrind $(TEST)

clean-callgrind:
	rm -f callgrind*

ab:
	ab -c 100 -n 10000 http://127.0.0.1:8080/

wget:
	wget -O - -v http://127.0.0.1:8080/
