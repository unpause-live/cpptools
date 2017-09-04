BASE=..
BUILD_DIR=$(BASE)/build
OUTPUT_DIR=$(BASE)/test/build

CFLAGS=-std=c++14 -pthread -c -fPIC -I$(BASE)/include -Wpedantic -Wall -Werror
LDFLAGS=-lpthread -L$(BUILD_DIR) 
CC=g++-5

ifeq ($(SANITIZE), 1)
EXTRA_CCFLAGS=-O0 -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls -g
EXTRA_LDFLAGS=-lasan -llsan
else
EXTRA_CCFLAGS=-O2
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