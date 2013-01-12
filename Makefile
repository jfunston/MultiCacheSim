# set MULTI_CACHE preprocessor macro to enable multiple caches
# this optimization provides ~10% speedup for single cache
# simulations.

CXX = g++
DEBUG_FLAGS = -O2 -g -Wall -Wextra -DDEBUG -std=gnu++11 -pthread
RELEASE_FLAGS= -O3 -march=native -Wall -Wextra -std=gnu++11 -pthread
CXXFLAGS=$(RELEASE_FLAGS)
DEPS=$(wildcard *.h) Makefile
OBJ=system.o cache.o seq_prefetch_system.o adj_prefetch_system.o threaded_sim.o

all: cache cache_threaded tags check cscope.out 

cache: main.cpp $(DEPS) $(OBJ)
	$(CXX) $(CXXFLAGS) -o cache main.cpp $(OBJ)

cache_threaded: main_threaded.cpp $(DEPS) $(OBJ)
	$(CXX) $(CXXFLAGS) -o cache_threaded main_threaded.cpp $(OBJ)

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
