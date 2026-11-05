CC=gcc
CFLAGS=-g -I. -Iinclude -Wall -Werror -Wpedantic
LDFLAGS=-g -Wall -Werror
LIBS=-lm

SOURCES=src/lexer.c src/parse.c
OBJECTS=$(patsubst src/%.c,build/%.o,$(SOURCES))

build/starcc.exe: $(OBJECTS) | buildDirs
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

build/%.o: src/%.c | buildDirs
	$(CC) -c $(CFLAGS) $< -o $@
build/%.o: src/lexer/%.c | buildDirs
	$(CC) $(CFLAGS) $< -o $@

buildDirs:
	mkdir -p build
	mkdir -p build/lexer

clean:
	rm -rf build

.PHONY: clean,buildDirs