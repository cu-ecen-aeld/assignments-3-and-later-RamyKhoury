CROSS_COMPILE ?= 
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -g
SRCS= writer.c
OBJS = build/writer.o
TARGET = writer

all: $(TARGET) file_type_get

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

build/%.o: %.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

file_type_get: $(TARGET)
	@file writer > ../assignments/assignment2/fileresult.txt

clean:
	@rm -rf build
	@rm -rf writer
	@echo "Cleaning ./finder-app/build"