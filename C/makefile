CC = gcc
CFLAGS = -Wall -g
LIBS = -lwebsockets
INCLUDES = -I./

SRCS = util.c asr.c
OBJS = util.o asr.o

TARGET = asr

# 默认目标
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

util.o: util.c util.h
	$(CC) $(CFLAGS) $(INCLUDES) -c util.c
asr.o: asr.c util.h
	$(CC) $(CFLAGS) $(INCLUDES) -c asr.c


run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean run