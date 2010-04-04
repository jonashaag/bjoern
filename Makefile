CC              = gcc
CFLAGS_NODEBUG  = -Wall -fno-strict-aliasing -shared
CFLAGS_NODEBUGP = $(CFLAGS_NODEBUG) -g
CFLAGS          = $(CFLAGS_NODEBUGP) -D DEBUG
CFLAGS_WARNALL  = $(CFLAGS) -Wextra
INCLUDE_DIRS    = -I /usr/include/python2.6/
LDFLAGS         = $(INCLUDE_DIRS) -l ev -I http-parser -l python2.6
PROFILE_CFLAGS  = $(CFLAGS) -pg
CFLAGS_OPTDEBUG = $(CFLAGS_NODEBUGP) -O3
CFLAGS_OPT      = $(CFLAGS_NODEBUG) -O3

OUTFILES		= _bjoern.so
FILES           = bjoern.c
FILES_NODEBUG   = $(FILES) http-parser/http_parser.o
FILES_DEBUG     = $(FILES) http-parser/http_parser_debug.o

all: clean
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(OUTFILES) $(FILES_DEBUG)

nodebugprints:
	$(CC) $(CFLAGS_NODEBUGP) $(LDFLAGS) -o $(OUTFILES) $(FILES_DEBUG)

prep:
	$(CC) $(LDFLAGS) -E bjoern.c | ${PAGER}

warnall:
	$(CC) $(CFLAGS_WARNALL) $(LDFLAGS) -o $(OUTFILES) $(FILES_DEBUG)

profile:
	$(CC) $(PROFILE_CFLAGS) $(LDFLAGS) -o $(OUTFILES) $(FILES_NODEBUG)

opt:
	$(CC) $(CFLAGS_OPT) $(LDFLAGS) -o $(OUTFILES) $(FILES_NODEBUG)
	strip $(OUTFILES)

optdebug:
	$(CC) $(CFLAGS_OPTDEBUG) $(LDFLAGS) -o $(OUTFILES) $(FILES_DEBUG)

clean:
	rm -f *.o
	rm -f *.pyc


run: nodebugprints
	python test.py

runwithdebug: all
	python test.py

gdb: all
	gdb python

valgrind: nodebugprints
	valgrind python test.py

callgrind: nodebugprints clean-callgrind
	valgrind --tool=callgrind python test.py

callgrind-opt: optdebug clean-callgrind
	valgrind --tool=callgrind python test.py

clean-callgrind:
	rm -f callgrind*

ab:
	ab -c 100 -n 10000 http://127.0.0.1:8080/
