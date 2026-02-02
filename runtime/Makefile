# Wyn Runtime Library Makefile

CC = clang
CFLAGS = -Wall -Wextra -O2 -fPIC
AR = ar
ARFLAGS = rcs

# Source files
SOURCES = system/system.c file/file.c time/time.c
OBJECTS = $(SOURCES:.c=.o)

# Output
LIBRARY = libwyn_runtime.a

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c wyn_runtime.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(LIBRARY)

install: $(LIBRARY)
	cp $(LIBRARY) ../lib/
	cp wyn_runtime.h ../include/

.PHONY: all clean install
