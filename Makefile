# vim :tabstop=8 :noexpandtab
EXTRA_CFLAGS	=

WANT_ROUTING	= yes
WANT_SENDFILE	= yes

FEATURES	=
ifeq ($(WANT_ROUTING), yes)
FEATURES	+= -D WANT_ROUTING
endif
ifeq ($(WANT_SENDFILE), yes)
FEATURES	+= -D WANT_SENDFILE
endif

CC		= gcc
CFLAGS_NODEBUG	= -std=c99 -Wall -fno-strict-aliasing -shared $(FEATURES) $(EXTRA_CFLAGS)
CFLAGS_NODEBUGP	= $(CFLAGS_NODEBUG) -g
CFLAGS		= $(CFLAGS_NODEBUGP) -D DEBUG
CFLAGS_OPTDEBUG	= $(CFLAGS_NODEBUGP) -O3
CFLAGS_OPT	= $(CFLAGS_NODEBUG) -O3
CLFAGS_OPTSMALL	= $(CFLAGS_NODEBUG) -Os
CFLAGS_WARNALL	= $(CFLAGS_OPT) -Wextra
INCLUDE_DIRS	= -I .				\
		  -I src			\
		  -I src/headers 		\
		  -I /usr/include/python2.6/	\
		  -I include 			\
		  -I include/http-parser
LDFLAGS		= -static $(INCLUDE_DIRS) -l ev -I http-parser -l python2.6

CC_ARGS		= $(LDFLAGS) -o $(OUTPUT_FILES)  $(SOURCE_FILES)

OUTPUT_FILES	= _bjoern.so

HTTP_PARSER_MODULE	= include/http-parser/http_parser_g.o
HTTP_PARSER_SOURCE	= include/http-parser/http_parser.c
SOURCE_FILES	= $(HTTP_PARSER_SOURCE)	\
		  src/bjoern.c

TEST		= python tests
PAGER		= less


all: clean
	$(CC) $(CFLAGS) $(CC_ARGS)

nodebugprints:
	$(CC) $(CFLAGS_NODEBUGP) $(CC_ARGS)

prep:
	$(CC) $(LDFLAGS) $(CFLAGS) -E $(FILES) | ${PAGER}

assembler:
	$(CC) $(LDFLAGS) $(CFLAGS_NODEBUGP) -S $(FILES)

assembleropt:
	$(CC) $(LDFLAGS) $(CFLAGS_OPTDEBUG) -S $(FILES)

warnall:
	$(CC) $(CFLAGS_WARNALL) $(CC_ARGS)

opt:
	$(CC) $(CFLAGS_OPT) $(CC_ARGS)
	strip $(OUTPUT_FILES)

optdebug:
	$(CC) $(CFLAGS_OPTDEBUG) $(CC_ARGS)
	strip $(OUTPUT_FILES)

optsmall:
	$(CC) $(CFLAGS_OPTSMALL) $(CC_ARGS)

clean:
	rm -f *.o
	rm -f *.pyc


run: nodebugprints
	$(TEST)

runwithdebug: all
	$(TEST)

DEBUGGER	= `which cgdb 2>/dev/null 1>&2 && echo -n c; echo gdb`
gdb: nodebugprints
	$(DEBUGGER) --args python tests
gdb-verbose: all
	$(DEBUGGER) --args python tests

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

get-http-parser:
	stuff/update-http-parser.sh
