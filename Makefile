SOURCE_DIR	= src
BUILD_DIR	= _build
objects		= $(patsubst $(SOURCE_DIR)/%.c, $(BUILD_DIR)/%.o, \
			     $(wildcard $(SOURCE_DIR)/*.c))

PYTHON_VERSION	= 2.6
PYTHON_DIR	= /usr/include/python$(PYTHON_VERSION)/

HTTP_PARSER_DIR	= include/http-parser
HTTP_PARSER_OBJ = $(HTTP_PARSER_DIR)/http_parser.o

CPPFLAGS	+= -I $(PYTHON_DIR) -I . -I $(SOURCE_DIR) -I $(HTTP_PARSER_DIR) 
CFLAGS		+= $(FEATURES) -std=c99 -Wall -Wno-unused -g
LDFLAGS		+= -l python2.6 -l ev -shared -static

ifneq ($(WANT_SENDFILE), no)
FEATURES	+= -D WANT_SENDFILE
endif
ifneq ($(WANT_ROUTING), no)
FEATURES	+= -D WANT_ROUTING
endif

all: prepare-build $(objects) bjoernmodule

bjoernmodule:
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(objects) $(HTTP_PARSER_OBJ) -o $(BUILD_DIR)/_bjoern.so
	python -c "import _bjoern"

again: clean all

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# foo.o: shortcut to $(BUILD_DIR)/foo.o
%.o: $(BUILD_DIR)/%.o
	

prepare-build:
	mkdir -p _build

clean:
	rm -f $(BUILD_DIR)/*

http_parser:
	stuff/make-http-parser

ab:
	ab -c 100 -n 10000 http://127.0.0.1:8080/

wget:
	wget -O - -q -S http://127.0.0.1:8080/

test:
	cd stuff && python ~/dev/projects/wsgitest/runner.py
