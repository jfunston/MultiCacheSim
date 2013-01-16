// Author: Justin Funston
// Date: January 2013
// Email: jfunston@sfu.ca

#ifndef PREFETCH_H
#define PREFETCH_H

class System;

// Base class and serves as the "null"
// prefetcher.
class Prefetch {
public:
   virtual int prefetchMiss(unsigned long long address, unsigned int tid, 
                              System* sys);
   virtual int prefetchHit(unsigned long long address, unsigned int tid,
                              System* sys);
};

// Modeling AMD's L1 prefetcher, a sequential
// line prefetcher. Primary difference is that
// thre real prefetcher has a dynamic prefetch width.
class SeqPrefetch : public Prefetch {
   unsigned long long lastMiss;
   unsigned long long lastPrefetch;
   static const unsigned int prefetchNum = 3;
public:
   int prefetchMiss(unsigned long long address, unsigned int tid, System* sys);
   int prefetchHit(unsigned long long address, unsigned int tid, System* sys);
};

// A simple adjacent line prefetcher
class AdjPrefetch : public Prefetch {
public:
   int prefetchMiss(unsigned long long address, unsigned int tid, System* sys);
   int prefetchHit(unsigned long long address, unsigned int tid, System* sys);
};

#endif
