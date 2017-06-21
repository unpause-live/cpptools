BASE=.
BUILD_DIR=$(BASE)/build

SRC=$(BASE)/src/async/AsyncQueue.cpp
OBJS=$(SRC:.cpp=.o)

CFLAGS=-std=c++14 -pthread -c -fPIC -O2 -I$(BASE)/include
LDFLAGS=-lpthread
CC=g++

all: libcpptools.so libcpptools.a headers

libcpptools.so: $(OBJS)
	mkdir -p $(BUILD_DIR)
	$(CC) -shared $(LDFLAGS) $(OBJS) -o $(BUILD_DIR)/$@

libcpptools.a: $(OBJS)
	mkdir -p $(BUILD_DIR)
	ar rcs $(BUILD_DIR)/$@ $^

headers:
	mkdir -p $(BUILD_DIR)
	cp -R $(BASE)/include $(BUILD_DIR)

clean:
	find $(BASE) -name "*.o" -type f -delete

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@
