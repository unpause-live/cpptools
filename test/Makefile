BASE=..
BUILD_DIR=$(BASE)/build
OUTPUT_DIR=$(BASE)/test/build

CFLAGS=-std=c++14 -pthread -c -fPIC -O0 -I$(BASE)/include -Wpedantic -Wall -Werror -g
LDFLAGS=-lpthread -L$(BUILD_DIR) 
CC=g++


all: async test

async: async.o
	mkdir -p $(OUTPUT_DIR)
	$(CC) async.o $(LDFLAGS) -o $(OUTPUT_DIR)/$@

test:
	./build/async

clean:
	rm *.o
	rm -rf $(OUTPUT_DIR)

%.o: %.cpp
	$(CC) $(CFLAGS) $< -o $@