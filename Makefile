# set MULTI_CACHE preprocessor macro to enable multiple caches
# this optimization provides ~10% speedup for single cache
# simulations.

CXX = g++
DEBUG_FLAGS = -O2 -g -Wall -Wextra -DDEBUG -std=gnu++11
RELEASE_FLAGS= -O3 -march=native -Wall -Wextra -std=gnu++11
CXXFLAGS=$(RELEASE_FLAGS)
DEPS=$(wildcard *.h) Makefile
OBJ=system.o cache.o seq_prefetch_system.o

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
	git push -u linode master

.PHONY: check
check:
	cppcheck --enable=all .

.PHONY: clean
clean:
	rm -f *.o cache tags cscope.out
