CROSS_COMPILE ?= 
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -g
SRCS= writer.c
OBJS = build/writer.o
TARGET = writer

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

build/%.o: %.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf build
	@rm -rf writer
	@echo "Cleaning ./finder-app/build"