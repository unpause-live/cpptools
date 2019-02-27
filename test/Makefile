BASE=..
BUILD_DIR=$(BASE)/build
OUTPUT_DIR=$(BASE)/test/build

CFLAGS=-std=c++17 -pthread -c -fPIC -I$(BASE)/include -Wpedantic -Wall -Werror -Wno-gnu-zero-variadic-macro-arguments
LDFLAGS=-lpthread
CC=g++

ifeq ($(SANITIZE), 1)
EXTRA_CCFLAGS=-O0 -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls -g 
EXTRA_LDFLAGS=-lasan -llsan
else
EXTRA_CCFLAGS=-Os
endif

all: async

async: async.o
	mkdir -p $(OUTPUT_DIR)
	$(CC) async.o $(EXTRA_LDFLAGS)  $(LDFLAGS) -o $(OUTPUT_DIR)/$@

test:
	./build/async

clean:
	rm *.o
	rm -rf $(OUTPUT_DIR)

%.o: %.cpp
	$(CC) $(CFLAGS) $(EXTRA_CCFLAGS) $< -o $@