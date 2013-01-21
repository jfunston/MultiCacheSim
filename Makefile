CXX = g++
DEBUG_FLAGS = -O2 -g -Wall -Wextra -DDEBUG -std=gnu++0x
RELEASE_FLAGS= -O3 -march=native -Wall -Wextra -std=gnu++0x
CXXFLAGS=$(RELEASE_FLAGS)
DEPS=$(wildcard *.h) Makefile
OBJ=system.o cache.o prefetch.o

all: cache tags check cscope.out 

cache: main.cpp $(DEPS) $(OBJ)
	$(CXX) $(CXXFLAGS) -o cache main.cpp $(OBJ)

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -o $@ -c $< 

tags: *.cpp *.h
	ctags *.cpp *.h

cscope.out: *.cpp *.h
	cscope -Rb

.PHONY: backup
backup:
	git push -u origin master

.PHONY: check
check:
	cppcheck --enable=all .

.PHONY: clean
clean:
	rm -f *.o cache tags cscope.out
