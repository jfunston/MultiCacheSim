CXX = g++
DEBUG_FLAGS = -O2 -g -Wall -Wextra -DDEBUG -std=gnu++14
RELEASE_FLAGS= -O3 -march=native -Wall -Wextra -std=gnu++14 -flto -static
CXXFLAGS=$(RELEASE_FLAGS)
DEPS=$(wildcard *.h) Makefile
OBJ=system.o cache.o prefetch.o
BUILD_DIR=$(shell pwd)

all: cache tags check tests/random tests/unit cscope.out 

cache: main.cpp $(DEPS) $(OBJ)
	$(CXX) $(CXXFLAGS) -o cache main.cpp $(OBJ)

tests/random: tests/random.cpp $(DEPS) $(OBJ)
	$(CXX) $(CXXFLAGS) -I$(BUILD_DIR) -o tests/random tests/random.cpp $(OBJ)

tests/unit: tests/unit.cpp $(DEPS) $(OBJ)
	$(CXX) $(DEBUG_FLAGS) -I$(BUILD_DIR) -o tests/unit tests/unit.cpp $(OBJ)

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -o $@ -c $< 

tags: *.cpp *.h
	ctags *.cpp *.h tests/*.cpp

cscope.out: *.cpp *.h
	cscope -Rb

.PHONY: check
check:
	cppcheck --enable=all .

.PHONY: clean
clean:
	rm -f *.o cache tags cscope.out
