SOURCE_DIR	= bjoern
BUILD_DIR	= _build
objects		= $(patsubst $(SOURCE_DIR)/%.c, $(BUILD_DIR)/%.o, \
			     $(wildcard $(SOURCE_DIR)/*.c))

PYTHON_VERSION	= 2.7
PYTHON_DIR	= /usr/include/python$(PYTHON_VERSION)/

HTTP_PARSER_DIR	= http-parser
HTTP_PARSER_OBJ = $(HTTP_PARSER_DIR)/http_parser.o
HTTP_PARSER_SRC = $(HTTP_PARSER_DIR)/http_parser.c

CPPFLAGS	+= -I $(PYTHON_DIR) -I . -I $(SOURCE_DIR) -I $(HTTP_PARSER_DIR)
CFLAGS		+= $(FEATURES) -std=c99 -fno-strict-aliasing -Wall -Wextra \
		   -Wno-unused -g -O3 -fPIC
LDFLAGS		+= -l python$(PYTHON_VERSION) -l ev -shared -static

ifneq ($(WANT_SENDFILE), no)
FEATURES	+= -D WANT_SENDFILE
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
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(objects) $(HTTP_PARSER_OBJ) -o $(BUILD_DIR)/bjoern.so
	python -c "import bjoern"

again: clean all

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# foo.o: shortcut to $(BUILD_DIR)/foo.o
%.o: $(BUILD_DIR)/%.o
	

prepare-build:
	mkdir -p _build

clean:
	rm -f $(BUILD_DIR)/*

ab:
	ab -c 100 -n 10000 'http://127.0.0.1:8080/a/b/c?k=v&k2=v2#fragment'

wget:
	wget -O - -q -S 'http://127.0.0.1:8080/a/b/c?k=v&k2=v2#fragment'

test:
	cd tests && python ~/dev/wsgitest/runner.py

valgrind:
	valgrind --leak-check=full --show-reachable=yes python tests/hello.py

callgrind:
	valgrind --tool=callgrind python tests/wsgitest-round-robin.py

memwatch:
	watch -n 0.5 \
	  'cat /proc/$$(pidof -s python)/cmdline | tr "\0" " " | head -c -1; \
	   echo; echo; \
	   tail -n +25 /proc/$$(pidof -s python)/smaps'
