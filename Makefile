# set MULTI_CACHE preprocessor macro to enable multiple caches
# this optimization provides ~10% speedup for single cache
# simulations.

DEBUG_FLAGS = -O2 -g -Wall -Wextra -DDEBUG
RELEASE_FLAGS = -O3 -march=native -Wall -Wextra
COMPILE_FLAGS=$(RELEASE_FLAGS)

all: cache tags check cscope.out 

cache: main.cpp cache.h cache.o Makefile
	g++ $(COMPILE_FLAGS) -o cache main.cpp cache.o

cache.o: cache.cpp cache.h Makefile
	g++ $(COMPILE_FLAGS) -c cache.cpp

tags: *.cpp *.h
	ctags *.cpp *.h

cscope.out: *.cpp *.h
	cscope -Rb

.PHONY: check
check:
	cppcheck --enable=all .

.PHONY: clean
clean:
	rm -f cache.o cache
