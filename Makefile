SOURCE_DIR	= bjoern
BUILD_DIR	= build

PYTHON_INCLUDE	= $(shell python2-config --include)
PYTHON_LDFLAGS	= $(shell python2-config --ldflags)

HTTP_PARSER_DIR	= http-parser
HTTP_PARSER_OBJ = $(HTTP_PARSER_DIR)/http_parser.o
HTTP_PARSER_SRC = $(HTTP_PARSER_DIR)/http_parser.c

objects		= $(HTTP_PARSER_OBJ) \
		  $(patsubst $(SOURCE_DIR)/%.c, $(BUILD_DIR)/%.o, \
		             $(wildcard $(SOURCE_DIR)/*.c))

CPPFLAGS	+= $(PYTHON_INCLUDE) -I . -I $(SOURCE_DIR) -I $(HTTP_PARSER_DIR)
CFLAGS		+= $(FEATURES) -std=c99 -fno-strict-aliasing -Wall -Wextra \
		   -Wno-unused -g -O0 -fPIC
LDFLAGS		+= $(PYTHON_LDFLAGS) -l ev -shared

ifneq ($(WANT_SENDFILE), no)
FEATURES	+= -D WANT_SENDFILE
endif

ifneq ($(WANT_SIGINT_HANDLING), no)
FEATURES	+= -D WANT_SIGINT_HANDLING
endif

all: prepare-build $(objects) bjoernmodule

print-env:
	@echo CFLAGS=$(CFLAGS)
	@echo CPPFLAGS=$(CPPFLAGS)
	@echo LDFLAGS=$(LDFLAGS)
	@echo args=$(HTTP_PARSER_SRC) $(wildcard $(SOURCE_DIR)/*.c)

opt: clean
	CFLAGS='-O3' make

small: clean
	CFLAGS='-Os' make

bjoernmodule:
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(objects) -o $(BUILD_DIR)/bjoern.so
	@PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) python2 -c "import bjoern"

again: clean all

debug:
	CFLAGS='-D DEBUG' make again

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c
	@echo ' -> ' $(CC) -c $< -o $@
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# foo.o: shortcut to $(BUILD_DIR)/foo.o
%.o: $(BUILD_DIR)/%.o


prepare-build:
	@mkdir -p $(BUILD_DIR)

clean:
	@rm -f $(BUILD_DIR)/*

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

test:
	cd tests && python2 ~/dev/wsgitest/runner.py

valgrind:
	valgrind --leak-check=full --show-reachable=yes python2 tests/empty.py

callgrind:
	valgrind --tool=callgrind python2 tests/wsgitest-round-robin.py

memwatch:
	watch -n 0.5 \
	  'cat /proc/$$(pgrep -n python2)/cmdline | tr "\0" " " | head -c -1; \
	   echo; echo; \
	   tail -n +25 /proc/$$(pgrep -n python2)/smaps'

upload:
	python2 setup.py sdist upload

$(HTTP_PARSER_OBJ):
	$(MAKE) -C $(HTTP_PARSER_DIR) http_parser.o CFLAGS_DEBUG_EXTRA=-fPIC CFLAGS_FAST_EXTRA=-fPIC
