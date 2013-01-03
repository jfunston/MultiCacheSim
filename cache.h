// Author: Justin Funston
// Date: November 2011
// Email: jfunston@sfu.ca

#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <map>
#include <set>
#include <list>

using namespace std;

#define PAGE_SIZE_4KB

#ifdef PAGE_SIZE_4KB
const unsigned long long PAGE_MASK = 0xFFFFFFFFFFFFF000;
const unsigned int PAGE_SHIFT = 12;
#elif defined PAGE_SIZE_2MB
const unsigned long long PAGE_MASK = 0xFFFFFFFFFFE00000;
const unsigned int PAGE_SHIFT = 21;
#else
#error "Bad PAGE_SIZE"
#endif

enum cacheState {MOD,OWN,EXC,SHA,INV};

struct cacheLine{
   unsigned long long tag;
   cacheState state;
   cacheLine(){tag = 0; state = INV;}
};

class Cache{
   vector<list<cacheLine> > sets;
public:
   Cache(unsigned int num_lines, unsigned int assoc);
   cacheState findTag(unsigned long long set, unsigned long long tag) const;
   void changeState(unsigned long long set, unsigned long long tag, cacheState state);
   void updateLRU(unsigned long long set, unsigned long long tag);
   bool checkWriteback(unsigned long long set, unsigned long long& tag) const;
   void insertLine(unsigned long long set, unsigned long long tag, cacheState state);
};   

class System{
   unsigned long long lastMiss;
   unsigned long long lastPrefetch;
   static const unsigned int prefetchNum = 3;
   unsigned long long SET_MASK;
   unsigned long long TAG_MASK;
   unsigned long long LINE_MASK;
   unsigned int SET_SHIFT;
   unsigned long long cycles;
   set<unsigned long long> seenLines; // Used for compulsory misses
   vector<unsigned long long> nextFreePage; //by domain
   vector<Cache> cpus;
   map<unsigned long long, unsigned int> pageList;
   vector<unsigned int> tid_to_domain;
   int prefetchMiss(unsigned long long address, unsigned int tid);
   int prefetchHit(unsigned long long address, unsigned int tid);
   int checkRemoteStates(unsigned long long set, unsigned long long tag, 
                        cacheState& state, unsigned int local);
   void updatePageList(unsigned long long address, unsigned int curDomain);
   void evictTraffic(unsigned long long set, unsigned long long tag, 
                     unsigned int local, bool isPrefetch);
   bool isLocal(unsigned long long address, unsigned int local);
   void setRemoteStates(unsigned long long set, unsigned long long tag, 
                        cacheState state, unsigned int local);
   void checkCompulsory(unsigned long long line);
public:
   System(unsigned int num_domains, vector<unsigned int> tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            bool count_compulsory=false);
   void memAccess(unsigned long long address, char rw, unsigned int tid, 
                     bool isPrefetch);
   unsigned long long hits, local_reads, remote_reads, othercache_reads,
      local_writes, remote_writes;
  unsigned long long compulsory;
  bool countCompulsory;
};


#endif
