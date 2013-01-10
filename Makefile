# set MULTI_CACHE preprocessor macro to enable multiple caches
# this optimization provides ~10% speedup for single cache
# simulations.

DEBUG_FLAGS = -O2 -g -Wall -Wextra -DDEBUG -std=gnu++11
RELEASE_FLAGS = -O3 -march=native -Wall -Wextra -std=gnu++11
COMPILE_FLAGS=$(RELEASE_FLAGS)
DEPS=$(wildcard *.h) Makefile
OBJ=system.o cache.o seq_prefetch_system.o

all: cache tags check cscope.out 

cache: main.cpp $(DEPS) $(OBJ)
	g++ $(COMPILE_FLAGS) -o cache main.cpp $(OBJ)

%.o: %.c $(DEPS)
	g++ $(COMPILE_FLAGS) -c -o $@ $<

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
