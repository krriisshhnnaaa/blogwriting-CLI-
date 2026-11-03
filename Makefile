CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -Isrc
LDFLAGS ?= -lcurl -lz

TARGET = blogwriter
SRCS = src/main.cpp src/writer.cpp src/ai.cpp src/gemini.cpp src/renderer.cpp src/editor.cpp src/parser.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o *.o $(TARGET)

.PHONY: all clean
