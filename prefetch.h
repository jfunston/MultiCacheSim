/*
Copyright (c) 2015 Justin Funston

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/

#ifndef PREFETCH_H
#define PREFETCH_H

#include <cstdint>

class System;

class Prefetch {
public:
   virtual int prefetchMiss(uint64_t address, unsigned int tid, 
                              System* sys)=0;
   virtual int prefetchHit(uint64_t address, unsigned int tid,
                              System* sys)=0;
};

//"Prefetcher" that does nothing
class NullPrefetch : public Prefetch {
public:
   int prefetchMiss(uint64_t address, unsigned int tid, 
                              System* sys);
   int prefetchHit(uint64_t address, unsigned int tid,
                              System* sys);
};

// Modeling AMD's L1 prefetcher, a sequential
// line prefetcher. Primary difference is that
// thre real prefetcher has a dynamic prefetch width.
class SeqPrefetch : public Prefetch {
   uint64_t lastMiss;
   uint64_t lastPrefetch;
   static const unsigned int prefetchNum = 3;
public:
   int prefetchMiss(uint64_t address, unsigned int tid, System* sys);
   int prefetchHit(uint64_t address, unsigned int tid, System* sys);
   SeqPrefetch()
   { lastMiss = lastPrefetch = 0;}
};

// A simple adjacent line prefetcher
class AdjPrefetch : public Prefetch {
public:
   int prefetchMiss(uint64_t address, unsigned int tid, System* sys);
   int prefetchHit(uint64_t address, unsigned int tid, System* sys);
};

#endif
