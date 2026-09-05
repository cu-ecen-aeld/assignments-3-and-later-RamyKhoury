CROSS_COMPILE ?= 
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Werror -Wall -Wextra -g
SRCS= aesdsocker.c
OBJS = build/aesdsocket.o
TARGET ?= aesdsocket
LDFLAGS ?= -lpthread -lrt

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

build/%.o: %.c
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

clean:
	@rm -rf build
	@rm -rf aesdsocket
	@echo "Cleaning ./server/build and ./server/aesdsocket"