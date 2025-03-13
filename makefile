CC = gcc
CFLAGS = -Wall -g
LIBS = -lwebsockets
INCLUDES = -I./

SRCS = util.c server.c
OBJS = util.o server.o

TARGET = server

# 默认目标
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

util.o: util.c util.h
	$(CC) $(CFLAGS) $(INCLUDES) -c util.c
server.o: server.c util.h
	$(CC) $(CFLAGS) $(INCLUDES) -c server.c


run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean run