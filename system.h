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

#ifndef SYSTEM_H
#define SYSTEM_H

#include <set>
#include <vector>
#include <map>
#include "misc.h"
#include "cache.h"
#include "prefetch.h"

// In separate struct so ThreadedSim
// doesn't need to repeat
// writes are counted when prefetching,
// other stats are not
struct SystemStats {
   unsigned long long hits;
   unsigned long long local_reads;
   unsigned long long remote_reads;
   unsigned long long othercache_reads;
   unsigned long long local_writes;
   unsigned long long remote_writes;
   unsigned long long compulsory;
   unsigned long long prefetched;
   SystemStats()
   {
      hits = local_reads = remote_reads = othercache_reads = 
         local_writes = remote_writes = compulsory = prefetched = 0;
   }
};

class System {
protected:
   friend class Prefetch;
   friend class SeqPrefetch;
   friend class AdjPrefetch;
   Prefetch* prefetcher;
   uint64_t SET_MASK;
   uint64_t TAG_MASK;
   uint64_t LINE_MASK;
   unsigned int SET_SHIFT;
   std::vector<unsigned int> tid_to_domain;
   //The cutoff associativity for using the deque cache implementation
   static const unsigned int assocCutoff = 64;

   // Used for compulsory misses
   std::set<uint64_t> seenLines;
   // Stores virtual to physical page mappings
   std::map<uint64_t, uint64_t> virtToPhysMap;
   // Used for determining new virtual to physical mappings
   uint64_t nextPage;
   bool countCompulsory;
   bool doAddrTrans;

   uint64_t virtToPhys(uint64_t address);
   void checkCompulsory(uint64_t line);
public:
   System(std::vector<unsigned int> tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            Prefetch* prefetcher, bool count_compulsory=false, 
            bool do_addr_trans=false);
   virtual void memAccess(uint64_t address, char rw, 
                           unsigned int tid, bool is_prefetch=false) = 0;
   SystemStats stats;
};

//For a system containing multiple caches
class MultiCacheSystem : public System {
   // Stores domain location of pages
   std::map<uint64_t, unsigned int> pageToDomain;
   std::vector<Cache*> caches;

   unsigned int checkRemoteStates(uint64_t set, uint64_t tag, 
                        cacheState& state, unsigned int local);
   void updatePageToDomain(uint64_t address, unsigned int curDomain);
   void setRemoteStates(uint64_t set, uint64_t tag, 
                        cacheState state, unsigned int local);
   void evictTraffic(uint64_t set, uint64_t tag, 
                     unsigned int local);
   bool isLocal(uint64_t address, unsigned int local);
   cacheState processMOESI(uint64_t set, uint64_t tag, 
                  cacheState remote_state, char rw, bool is_prefetch, 
                  bool local_traffic, unsigned int local, unsigned int remote);
public:
   MultiCacheSystem(std::vector<unsigned int> tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            Prefetch* prefetcher, bool count_compulsory=false, 
            bool do_addr_trans=false, unsigned int num_domains=1);
   ~MultiCacheSystem();
   void memAccess(uint64_t address, char rw, unsigned int tid, 
                     bool is_prefetch=false);
};

//For a system containing a sinle cache
//  performs about 10% better than the MultiCache implementation
class SingleCacheSystem : public System {
   Cache* cache;

public:
   SingleCacheSystem(std::vector<unsigned int> tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            Prefetch* prefetcher, bool count_compulsory=false, 
            bool do_addr_trans=false);
   ~SingleCacheSystem();
   void memAccess(uint64_t address, char rw, unsigned int tid, 
                     bool is_prefetch=false);
};

#endif

