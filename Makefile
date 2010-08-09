SOURCE_DIR	= src
OBJ_DIR		= _build
objects		= $(patsubst $(SOURCE_DIR)/%.c, $(OBJ_DIR)/%.o, \
			     $(wildcard $(SOURCE_DIR)/*.c))

PYTHON_VERSION	= 2.6
PYTHON_DIR	= /usr/include/python$(PYTHON_VERSION)

HTTP_PARSER_DIR	= include/http-parser

CPPFLAGS	+= -I $(PYTHON_DIR) -I . -I $(SOURCE_DIR) -I $(HTTP_PARSER_DIR) 
CFLAGS		+= -l ev -static $(FEATURES) -std=c99 -Wall -Wno-unused
CC_		= $(CC) $(CPPFLAGS) $(CFLAGS)

ifneq ($(WANT_SENDFILE), no)
FEATURES	+= -D WANT_SENDFILE
endif
ifneq ($(WANT_ROUTING), no)
FEATURES	+= -D WANT_ROUTING
endif

all: $(objects)

again: clean all

$(OBJ_DIR)/%.o: $(SOURCE_DIR)/%.c
	$(CC_) -c $< -o $@

$(OBJ_DIR)/parser.o:
	$(CC_) -l http_parser -c $(SOURCE_DIR)/parser.c -o $(OBJ_DIR)/parser.o

# foo.o: shortcut to $(OBJ_DIR)/foo.o
%.o: $(OBJ_DIR)/%.o
	

clean:
	rm -f $(OBJ_DIR)/*.o
