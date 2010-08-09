SOURCE_DIR	= src
OBJ_DIR		= _build
objects		= $(patsubst $(SOURCE_DIR)/%.c, $(OBJ_DIR)/%.o, \
			     $(wildcard $(SOURCE_DIR)/*.c))

PYTHON_VERSION	= 2.6
PYTHON_DIR	= /usr/include/python$(PYTHON_VERSION)/

HTTP_PARSER_DIR	= include/http-parser
HTTP_PARSER_OBJ = $(HTTP_PARSER_DIR)/http_parser.o

CPPFLAGS	+= -I $(PYTHON_DIR) -I . -I $(SOURCE_DIR) -I $(HTTP_PARSER_DIR) 
CFLAGS		+= $(FEATURES) -std=c99 -Wall -Wno-unused
LDFLAGS		+= -l python2.6 -l ev -shared -static

ifneq ($(WANT_SENDFILE), no)
FEATURES	+= -D WANT_SENDFILE
endif
ifneq ($(WANT_ROUTING), no)
FEATURES	+= -D WANT_ROUTING
endif

all: $(objects) bjoernmodule

bjoernmodule:
	$(CC) $(CPPFLAGS) $(LDFLAGS) $(objects) $(HTTP_PARSER_OBJ) -o _bjoern.so
	python -c "import _bjoern"

again: clean all

$(OBJ_DIR)/%.o: $(SOURCE_DIR)/%.c prepare-build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# foo.o: shortcut to $(OBJ_DIR)/foo.o
%.o: $(OBJ_DIR)/%.o
	

prepare-build:
	mkdir -p _build

clean:
	rm -f $(OBJ_DIR)/*.o

http_parser:
	stuff/make-http-parser
