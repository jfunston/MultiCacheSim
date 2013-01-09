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
   vector<Cache*> cpus;
   map<unsigned long long, unsigned long long> pageList;
   unsigned long long nextPage;
   vector<unsigned int> tid_to_domain;
   bool countCompulsory;
   bool doAddrTrans;

   int prefetchMiss(unsigned long long address, unsigned int tid);
   int prefetchHit(unsigned long long address, unsigned int tid);
   int checkRemoteStates(unsigned long long set, unsigned long long tag, 
                        cacheState& state, unsigned int local);
   //TODO: Currently updatePageList and virtToPhys are mutually exclusive:
   //       both use pageList
   void updatePageList(unsigned long long address, unsigned int curDomain);
   unsigned long long virtToPhys(unsigned long long address);
   void evictTraffic(unsigned long long set, unsigned long long tag, 
                     unsigned int local, bool isPrefetch);
   bool isLocal(unsigned long long address, unsigned int local);
   void setRemoteStates(unsigned long long set, unsigned long long tag, 
                        cacheState state, unsigned int local);
   void checkCompulsory(unsigned long long line);
public:
   System(unsigned int num_domains, vector<unsigned int> tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            bool count_compulsory=false, bool do_addr_trans=false);
   ~System();
   void memAccess(unsigned long long address, char rw, unsigned int tid, 
                     bool isPrefetch);
   unsigned long long hits, local_reads, remote_reads, othercache_reads,
      local_writes, remote_writes;
   unsigned long long compulsory;
};

#endif
