/*
Copyright (c) 2015-2018 Justin Funston

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

#pragma once

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <cstdint>

#include "misc.h"
#include "cache.h"
#include "prefetch.h"

struct SystemStats {
   uint64_t hits{0};
   uint64_t local_reads{0};
   uint64_t remote_reads{0};
   uint64_t othercache_reads{0};
   uint64_t local_writes{0};
   uint64_t remote_writes{0};
   uint64_t compulsory{0};
   uint64_t prefetched{0};
};

class System {
protected:
   friend class Prefetch;
   friend class AdjPrefetch;
   friend class SeqPrefetch;
   std::unique_ptr<Prefetch> prefetcher;
   uint64_t setMask;
   uint64_t tagMask;
   uint64_t lineMask;
   uint32_t setShift;
   std::vector<unsigned int>& tidToDomain;

   // Used for compulsory misses
   std::unordered_set<uint64_t> seenLines;
   // Stores virtual to physical page mappings
   std::unordered_map<uint64_t, uint64_t> virtToPhysMap;
   // Used for determining new virtual to physical mappings
   uint64_t nextPage{0};
   bool countCompulsory;
   bool doAddrTrans;

   uint64_t virtToPhys(uint64_t address);
   void checkCompulsory(uint64_t line);
public:
   virtual ~System() = default;
   System(std::vector<unsigned int>& tid_to_domain,
          unsigned int line_size, unsigned int num_lines, unsigned int assoc,
          std::unique_ptr<Prefetch> prefetcher, bool count_compulsory=false, 
          bool do_addr_trans=false);
   virtual void memAccess(uint64_t address, char rw, 
                          unsigned int tid, bool is_prefetch=false) = 0;
   SystemStats stats;
};

//For a system containing multiple caches
class MultiCacheSystem : public System {
private:
   // Stores NUMA domain location of pages
   std::unordered_map<uint64_t, unsigned int> pageToDomain;
   std::vector<std::unique_ptr<Cache>> caches;

   unsigned int checkRemoteStates(uint64_t set, uint64_t tag, 
                        CacheState& state, unsigned int local);
   void updatePageToDomain(uint64_t address, unsigned int curDomain);
   void setRemoteStates(uint64_t set, uint64_t tag, 
                        CacheState state, unsigned int local);
   void evictTraffic(uint64_t set, uint64_t tag, 
                     unsigned int local);
   bool isLocal(uint64_t address, unsigned int local);
   CacheState processMOESI(uint64_t set, uint64_t tag, 
                  CacheState remote_state, char rw, bool is_prefetch, 
                  bool local_traffic, unsigned int local, unsigned int remote);
public:
   MultiCacheSystem(std::vector<unsigned int>& tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            std::unique_ptr<Prefetch> prefetcher, bool count_compulsory=false, 
            bool do_addr_trans=false, unsigned int num_domains=1);

   void memAccess(uint64_t address, char rw, unsigned int tid, 
                     bool is_prefetch=false) override;
};

// For a system containing a sinle cache
// performs about 10% better than the MultiCache implementation
class SingleCacheSystem : public System {
public:
   SingleCacheSystem(std::vector<unsigned int>& tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            std::unique_ptr<Prefetch> prefetcher, bool count_compulsory=false, 
            bool do_addr_trans=false);

   void memAccess(uint64_t address, char rw, unsigned int tid, 
                     bool is_prefetch=false) override;
private:
   std::unique_ptr<Cache> cache;
};
