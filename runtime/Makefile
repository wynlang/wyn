# Wyn Runtime Library Makefile

CC = clang
CFLAGS = -Wall -Wextra -O2 -fPIC -I../src
AR = ar
ARFLAGS = rcs

# Source files
SOURCES = system/system.c file/file.c time/time.c core/string_concat.c c_parser_api.c
OBJECTS = $(SOURCES:.c=.o)

# Output
LIBRARY = libwyn_runtime.a

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c wyn_runtime.h
	$(CC) $(CFLAGS) -c $< -o $@

# Special rule for c_parser_api.c which needs access to parser headers
c_parser_api.o: c_parser_api.c wyn_runtime.h ../src/common.h ../src/ast.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(LIBRARY)

install: $(LIBRARY)
	cp $(LIBRARY) ../lib/
	cp wyn_runtime.h ../include/

.PHONY: all clean install
