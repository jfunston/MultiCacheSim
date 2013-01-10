# set MULTI_CACHE preprocessor macro to enable multiple caches
# this optimization provides ~10% speedup for single cache
# simulations.

DEBUG_FLAGS = -O2 -g -Wall -Wextra -DDEBUG -std=gnu++11
RELEASE_FLAGS = -O3 -march=native -Wall -Wextra -std=gnu++11
COMPILE_FLAGS=$(RELEASE_FLAGS)

all: cache tags check cscope.out 

cache: main.cpp system.h cache.o system.o Makefile
	g++ $(COMPILE_FLAGS) -o cache main.cpp cache.o system.o

system.o: system.cpp cache.h system.h misc.h Makefile
	g++ $(COMPILE_FLAGS) -c system.cpp

cache.o: cache.cpp cache.h misc.h Makefile
	g++ $(COMPILE_FLAGS) -c cache.cpp

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
