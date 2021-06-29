SOURCE_DIR	= bjoern
BUILD_DIR	= build
PYTHON	?= python2

PYTHON_INCLUDE	= $(shell ${PYTHON}-config --includes)
PYTHON_LDFLAGS	= $(shell ${PYTHON}-config --ldflags)

HTTP_PARSER_DIR	= http-parser
HTTP_PARSER_OBJ = $(HTTP_PARSER_DIR)/http_parser.o
HTTP_PARSER_SRC = $(HTTP_PARSER_DIR)/http_parser.c

STATSD_CLIENT_DIR = statsd-c-client
STATSD_CLIENT_OBJ = $(STATSD_CLIENT_DIR)/statsd-client.o
STATSD_CLIENT_SRC = $(STATSD_CLIENT_DIR)/statsd-client.c

objects		= $(HTTP_PARSER_OBJ) $(STATSD_CLIENT_OBJ) \
		  $(patsubst $(SOURCE_DIR)/%.c, $(BUILD_DIR)/%.o, \
		             $(wildcard $(SOURCE_DIR)/*.c))

CPPFLAGS	+= $(PYTHON_INCLUDE) -I . -I $(SOURCE_DIR) -I $(HTTP_PARSER_DIR) -I $(STATSD_CLIENT_DIR)
CFLAGS		+= $(FEATURES) -std=c99 -fno-strict-aliasing -fcommon -fPIC -Wall
LDFLAGS		+= $(PYTHON_LDFLAGS) -l ev -shared -fcommon

ifneq ($(WANT_SIGINT_HANDLING), no)
FEATURES	+= -D WANT_SIGINT_HANDLING
endif

ifneq ($(WANT_SIGNAL_HANDLING), no)
FEATURES	+= -D WANT_SIGNAL_HANDLING
endif

ifndef SIGNAL_CHECK_INTERVAL
FEATURES	+= -D SIGNAL_CHECK_INTERVAL=0.1
endif

ifeq ($(WANT_STATSD), yes)
FEATURES	+= -D WANT_STATSD
else
filter_out	= $(foreach v,$(2),$(if $(findstring $(1),$(v)),,$(v)))
objects		:= $(call filter_out,statsd,$(objects))
endif

ifeq ($(WANT_STATSD_TAGS), yes)
FEATURES	+= -D WANT_STATSD_TAGS
endif

all: prepare-build $(objects) _bjoernmodule

print-env:
	@echo CFLAGS=$(CFLAGS)
	@echo CPPFLAGS=$(CPPFLAGS)
	@echo LDFLAGS=$(LDFLAGS)
	@echo args=$(HTTP_PARSER_SRC) $(wildcard $(SOURCE_DIR)/*.c)

opt: clean
	CFLAGS='-O3' make

small: clean
	CFLAGS='-Os' make

_bjoernmodule:
	@echo ' -> ' $(CC) $(CPPFLAGS) $(CFLAGS) $(objects) $(LDFLAGS) -o $(BUILD_DIR)/_bjoern.so
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(objects) $(LDFLAGS) -o $(BUILD_DIR)/_bjoern.so
	@PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) ${PYTHON} -c "import bjoern"

again: clean all

debug:
	CFLAGS='${CFLAGS} -D DEBUG -g' make again

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c
	@echo ' -> ' $(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# foo.o: shortcut to $(BUILD_DIR)/foo.o
%.o: $(BUILD_DIR)/%.o


prepare-build:
	@mkdir -p $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)/*

distclean:
	@rm -rf dist/

AB		= ab -c 100 -n 10000
TEST_URL	= "http://127.0.0.1:8080/a/b/c?k=v&k2=v2"

ab: ab1 ab2 ab3 ab4

ab1:
	$(AB) $(TEST_URL)
ab2:
	@echo 'asdfghjkl=asdfghjkl&qwerty=qwertyuiop' > /tmp/bjoern-post.tmp
	$(AB) -p /tmp/bjoern-post.tmp $(TEST_URL)
ab3:
	$(AB) -k $(TEST_URL)
ab4:
	@echo 'asdfghjkl=asdfghjkl&qwerty=qwertyuiop' > /tmp/bjoern-post.tmp
	$(AB) -k -p /tmp/bjoern-post.tmp $(TEST_URL)

wget:
	wget -O - -q -S $(TEST_URL)

valgrind:
	valgrind --leak-check=full --show-reachable=yes ${PYTHON} tests/empty.py

callgrind:
	valgrind --tool=callgrind ${PYTHON} tests/wsgitest-round-robin.py

memwatch:
	watch -n 0.5 \
	  'cat /proc/$$(pgrep -n ${PYTHON})/cmdline | tr "\0" " " | head -c -1; \
	   echo; echo; \
	   tail -n +25 /proc/$$(pgrep -n ${PYTHON})/smaps'

packages:
	${PYTHON} setup.py sdist
	$(MAKE) -C packaging all

upload: packages
	twine upload dist/*.tar.gz dist/*.whl

$(HTTP_PARSER_OBJ):
	$(MAKE) -C $(HTTP_PARSER_DIR) http_parser.o CFLAGS_DEBUG_EXTRA=-fPIC CFLAGS_FAST_EXTRA=-fPIC

$(STATSD_CLIENT_OBJ):
	$(MAKE) -C $(STATSD_CLIENT_DIR) statsd-client.o CFLAGS=-fPIC
