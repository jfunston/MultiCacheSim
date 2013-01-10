// Author: Justin Funston
// Date: January 2013
// Email: jfunston@sfu.ca

#ifndef SYSTEM_H
#define SYSTEM_H

#include <set>
#include <vector>
#include <map>
#include "misc.h"
#include "cache.h"

class System {
protected:
   unsigned long long SET_MASK;
   unsigned long long TAG_MASK;
   unsigned long long LINE_MASK;
   unsigned int SET_SHIFT;
   vector<Cache*> cpus;
   vector<unsigned int> tid_to_domain;

   set<unsigned long long> seenLines; // Used for compulsory misses
   map<unsigned long long, unsigned long long> pageList;
   unsigned long long nextPage;
   bool countCompulsory;
   bool doAddrTrans;

   int checkRemoteStates(unsigned long long set, unsigned long long tag, 
                        cacheState& state, unsigned int local);
   //TODO: Currently updatePageList and virtToPhys are mutually exclusive
   //       because both use pageList
   void updatePageList(unsigned long long address, unsigned int curDomain);
   unsigned long long virtToPhys(unsigned long long address);
   void evictTraffic(unsigned long long set, unsigned long long tag, 
                     unsigned int local, bool is_prefetch);
   bool isLocal(unsigned long long address, unsigned int local);
   void setRemoteStates(unsigned long long set, unsigned long long tag, 
                        cacheState state, unsigned int local);
   void checkCompulsory(unsigned long long line);
   cacheState processMESI(cacheState remote_state, char rw,
      bool is_prefetch, bool local_traffic);
public:
   System(unsigned int num_domains, vector<unsigned int> tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            bool count_compulsory=false, bool do_addr_trans=false);
   virtual void memAccess(unsigned long long address, char rw, unsigned int tid, 
                     bool is_prefetch) = 0;
   virtual ~System();
   unsigned long long hits, local_reads, remote_reads, othercache_reads,
      local_writes, remote_writes, compulsory;
};

// Modelling AMD's L1 Prefetcher
class SeqPrefetchSystem : public System {
   unsigned long long lastMiss;
   unsigned long long lastPrefetch;
   static const unsigned int prefetchNum = 3;

   int prefetchMiss(unsigned long long address, unsigned int tid);
   int prefetchHit(unsigned long long address, unsigned int tid);
public:
   SeqPrefetchSystem(unsigned int num_domains, vector<unsigned int> tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            bool count_compulsory=false, bool do_addr_trans=false);
   void memAccess(unsigned long long address, char rw, unsigned int tid, 
                     bool is_prefetch);
};

// Modelling a simple adjacent line prefetcher
// Doesn't implement compulsary miss counting
// or address transalation (though it would be
// easy to add)
class AdjPrefetchSystem : public System {
public:
   AdjPrefetchSystem(unsigned int num_domains, vector<unsigned int> tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc);
   void memAccess(unsigned long long address, char rw, unsigned int tid, 
                     bool is_prefetch);
};

#endif
