CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
CXXFLAGS += -Isrc -MMD -MP
LDLIBS = -lcurl -lz

TARGET = blogwriter
SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o src/*.d $(TARGET)

-include $(DEPS)

.PHONY: all clean
