CXX = g++
CXXFLAGS = -Wall -g
LIBS = -lwebsockets
INCLUDES = -I./

SRCS = asr.cpp
OBJS = asr.o

TARGET = asr

# 默认目标
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

asr.o: asr.cpp asr.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c asr.cpp

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean run

