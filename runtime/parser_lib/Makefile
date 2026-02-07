# Wyn C Parser Library Makefile
# Builds the C parser as a static library for use by Wyn programs

CC = clang
CFLAGS = -Wall -Wextra -O2 -fPIC -I../../src
AR = ar
ARFLAGS = rcs

SRC_DIR = ../../src

# Source files from the compiler
LEXER_SRC = $(SRC_DIR)/lexer.c
PARSER_SRC = $(SRC_DIR)/parser.c
ERROR_SRC = $(SRC_DIR)/error.c
SECURITY_SRC = $(SRC_DIR)/security.c

# Our API wrapper
API_SRC = parser_api.c

# Object files
OBJS = lexer.o parser.o error.o security.o parser_api.o

# Output
LIBRARY = libwyn_c_parser.a

all: $(LIBRARY)

lexer.o: $(LEXER_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

parser.o: $(PARSER_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

error.o: $(ERROR_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

security.o: $(SECURITY_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

parser_api.o: $(API_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBRARY): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -f *.o $(LIBRARY)

.PHONY: all clean
