CC = clang
CFLAGS = -std=gnu17 -lpthread
TARGET = target/server
SRC = src/main.c

	
all: debug

debug: target
	$(CC) $(SRC) $(CFLAGS) -Wall -fsanitize=address -g -o $(TARGET)

release: target
	$(CC) $(SRC) -O2 $(CFLAGS) -o $(TARGET)

target:
	mkdir -p target

.PHONY: debug release run
