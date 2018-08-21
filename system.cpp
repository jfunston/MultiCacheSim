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

#include <cassert>
#include <cmath>
#include <cstdlib>

#include "misc.h"
#include "cache.h"
#include "system.h"

System::System(
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            std::unique_ptr<Prefetch> prefetcher, 
            bool count_compulsory /*=false*/,
            bool do_addr_trans /*=false*/) :
            prefetcher(std::move(prefetcher)),
            countCompulsory(count_compulsory),
            doAddrTrans(do_addr_trans)
{
   assert(num_lines % assoc == 0);

   lineMask = ((uint64_t) line_size)-1;
   setShift = log2(line_size);
   setMask = ((num_lines / assoc) - 1) << setShift;
   tagMask = ~(setMask | lineMask);
}

void System::checkCompulsory(uint64_t line)
{
   if(!seenLines.count(line)) {
      stats.compulsory++;
      seenLines.insert(line);
   }
}

uint64_t System::virtToPhys(uint64_t address)
{
   uint64_t phys_addr = address & (~pageMask);
   uint64_t virt_page = address & pageMask;
   auto it = virtToPhysMap.find(virt_page);

   if(it != virtToPhysMap.end()) {
      uint64_t phys_page = it->second;
      phys_addr |= phys_page;
   }
   else {
      uint64_t phys_page = nextPage << pageShift;
      phys_addr |= phys_page;
      virtToPhysMap.insert(std::make_pair(virt_page, phys_page));
      ++nextPage;
   }

   return phys_addr;
}

unsigned int MultiCacheSystem::checkRemoteStates(uint64_t set, 
               uint64_t tag, CacheState& state, unsigned int local)
{
   CacheState curState = CacheState::Invalid;
   state = CacheState::Invalid;
   unsigned int remote = 0;

   for(unsigned int i=0; i<caches.size(); ++i) {
      if(i == local) {
         continue;
      }

      curState = caches[i]->findTag(set, tag);
      switch (curState)
      {
         case CacheState::Owned:
            state = CacheState::Owned;
            return i;
            break;
         case CacheState::Shared:
            // A cache line in a shared state may be
            // in the owned state in a different cache
            // so don't return i immdiately
            state = CacheState::Shared;
            remote = i;
            break;
         case CacheState::Exclusive:
            state = CacheState::Exclusive;
            return i;
            break;
         case CacheState::Modified:
            state = CacheState::Modified;
            return i;
            break;
         default:
            break;
      }
   }

   return remote;
}

void MultiCacheSystem::setRemoteStates(uint64_t set, 
               uint64_t tag, CacheState state, unsigned int local)
{
   for(unsigned int i=0; i < caches.size(); ++i) {
      if(i != local) {
         caches[i]->changeState(set, tag, state);
      }
   }
}

// Maintains the statistics for memory write-backs
void MultiCacheSystem::evictTraffic(uint64_t set, 
               uint64_t tag, unsigned int local)
{
   uint64_t page = ((set << setShift) | tag) & pageMask;

#ifdef DEBUG
   const auto it = pageList.find(page);
   assert(it != pageList.end());
#endif

   unsigned int domain = pageToDomain[page];
   if(domain == local) {
      stats.local_writes++;
   } else {
      stats.remote_writes++;
   }
}

bool MultiCacheSystem::isLocal(uint64_t address, unsigned int local)
{
   uint64_t page = address & pageMask;

#ifdef DEBUG
   const auto it = pageList.find(page);
   assert(it != pageList.end());
#endif

   unsigned int domain = pageToDomain[page];
   return (domain == local);
}


CacheState MultiCacheSystem::processMOESI(uint64_t set,
                  uint64_t tag, CacheState remote_state, AccessType accessType, 
                  bool local_traffic, unsigned int local, 
                  unsigned int remote)
{
   CacheState new_state = CacheState::Invalid;
   bool is_prefetch = (accessType == AccessType::Prefetch);

   if (remote_state == CacheState::Invalid && accessType == AccessType::Read) {
      new_state = CacheState::Exclusive;

      if (local_traffic && !is_prefetch) {
         stats.local_reads++;
      } else if (!is_prefetch) {
         stats.remote_reads++;
      }
   }
   else if (remote_state == CacheState::Invalid && accessType == AccessType::Write) {
      new_state = CacheState::Modified;

      if (local_traffic && !is_prefetch) {
         stats.local_reads++;
      } else if (!is_prefetch) {
         stats.remote_reads++;
      }
   }
   else if (remote_state == CacheState::Shared && accessType == AccessType::Read) {
      new_state = CacheState::Shared;

      if (local_traffic && !is_prefetch) {
         stats.local_reads++;
      } else if (!is_prefetch) {
         stats.remote_reads++;
      }
   }
   else if (remote_state == CacheState::Shared && accessType == AccessType::Write) {
      new_state = CacheState::Modified;
      setRemoteStates(set, tag, CacheState::Invalid, local);

      if (!is_prefetch) {
         stats.othercache_reads++;
      }
   }
   else if ((remote_state == CacheState::Modified || 
             remote_state == CacheState::Owned) && accessType == AccessType::Read) {
      new_state = CacheState::Shared;
      caches[remote]->changeState(set, tag, CacheState::Owned);

      if (!is_prefetch) {
         stats.othercache_reads++;
      }
   }
   else if ((remote_state == CacheState::Modified || 
             remote_state == CacheState::Owned || 
             remote_state == CacheState::Exclusive) 
             && accessType == AccessType::Write) {
      new_state = CacheState::Modified;
      setRemoteStates(set, tag, CacheState::Invalid, local);

      if (!is_prefetch) {
         stats.othercache_reads++;
      }
   }
   else if (remote_state == CacheState::Exclusive && accessType == AccessType::Read) {
      new_state = CacheState::Shared;
      caches[remote]->changeState(set, tag, CacheState::Shared);

      if (!is_prefetch) {
         stats.othercache_reads++;
      }
   }

#ifdef DEBUG
   assert(new_state != CacheState::Invalid);
#endif

   return new_state;
}

void MultiCacheSystem::memAccess(uint64_t address, AccessType accessType, 
      unsigned int tid)
{
   if (doAddrTrans) {
      address = virtToPhys(address);
   }

   if (accessType != AccessType::Prefetch) {
      stats.accesses++;
   }

   unsigned int local = tidToDomain[tid];
   updatePageToDomain(address, local);

   uint64_t set = (address & setMask) >> setShift;
   uint64_t tag = address & tagMask;
   CacheState state = caches[local]->findTag(set, tag);
   bool hit = (state != CacheState::Invalid);

   if (countCompulsory && accessType != AccessType::Prefetch) {
      checkCompulsory(address & (~lineMask));
   }

   // Handle hits 
   if (accessType == AccessType::Write && hit) { 
      caches[local]->changeState(set, tag, CacheState::Modified);
      setRemoteStates(set, tag, CacheState::Invalid, local);
   }

   if (hit) {
      caches[local]->updateLRU(set, tag);

      if (accessType != AccessType::Prefetch) {
         stats.hits++;
         if (prefetcher) {
            stats.prefetched += prefetcher->prefetchHit(address, tid, *this);
         }
      }
   }
   else {
      // Now handle miss cases

      CacheState remote_state;
      unsigned int remote = checkRemoteStates(set, tag, remote_state, local);

      uint64_t evicted_tag;
      bool writeback = caches[local]->checkWriteback(set, evicted_tag);
      // TODO both evictTraffic and isLocal search the the pageToDomain map
      if (writeback) {
         evictTraffic(set, evicted_tag, local);
      }

      bool local_traffic = isLocal(address, local);
      CacheState new_state = processMOESI(set, tag, remote_state, accessType, 
                                 local_traffic, local, remote);
      caches[local]->insertLine(set, tag, new_state);

      if (accessType == AccessType::Prefetch && prefetcher) {
         stats.prefetched += prefetcher->prefetchMiss(address, tid, *this);
      }
   }
}

// Keeps track of which NUMA domain each memory page is in,
// using a first-touch policy
void MultiCacheSystem::updatePageToDomain(uint64_t address, 
                                          unsigned int curDomain)
{
   uint64_t page = address & pageMask;

   const auto it = pageToDomain.find(page);
   if(it == pageToDomain.end()) {
      pageToDomain[page] = curDomain;
   }
}

MultiCacheSystem::MultiCacheSystem(std::vector<unsigned int>& tid_to_domain, 
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            std::unique_ptr<Prefetch> prefetcher, bool count_compulsory /*=false*/,
            bool do_addr_trans /*=false*/, unsigned int num_domains) : 
            System(line_size, num_lines, assoc, std::move(prefetcher), 
                     count_compulsory, do_addr_trans),
            tidToDomain(tid_to_domain)
{
   caches.reserve(num_domains);

   for (unsigned int i=0; i<num_domains; ++i) {
      caches.push_back(std::make_unique<Cache>(num_lines, assoc));
   }
}

void SingleCacheSystem::memAccess(uint64_t address, AccessType accessType, unsigned int tid)
{
   bool is_prefetch = (accessType == AccessType::Prefetch);

   if (doAddrTrans) {
      address = virtToPhys(address);
   }

   if (!is_prefetch) {
      stats.accesses++;
   }

   uint64_t set = (address & setMask) >> setShift;
   uint64_t tag = address & tagMask;
   CacheState state = cache->findTag(set, tag);
   bool hit = (state != CacheState::Invalid);

   if (countCompulsory && !is_prefetch) {
      checkCompulsory(address & ~lineMask);
   }

   // Handle hits 
   if (accessType == AccessType::Write && hit) {  
      cache->changeState(set, tag, CacheState::Modified);
   }

   if (hit) {
      cache->updateLRU(set, tag);

      if (!is_prefetch) {
         stats.hits++;
         if (prefetcher) {
            stats.prefetched += prefetcher->prefetchHit(address, tid, *this);
         }
      }

      return;
   }

   CacheState new_state = CacheState::Invalid;
   uint64_t evicted_tag;
   bool writeback = cache->checkWriteback(set, evicted_tag);

   if (writeback) {
      stats.local_writes++;
   }

   if (accessType == AccessType::Read) {
      new_state = CacheState::Exclusive;
   } else {
      new_state = CacheState::Modified;
   }

   if (!is_prefetch) {
      stats.local_reads++;
   }

   cache->insertLine(set, tag, new_state);
   if (!is_prefetch && prefetcher) {
      stats.prefetched += prefetcher->prefetchMiss(address, tid, *this);
   }
}

SingleCacheSystem::SingleCacheSystem( 
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            std::unique_ptr<Prefetch> prefetcher, 
            bool count_compulsory /*=false*/,
            bool do_addr_trans /*=false*/) : 
            System(line_size, num_lines, assoc,
               std::move(prefetcher), count_compulsory, do_addr_trans), 
            cache(std::make_unique<Cache>(num_lines, assoc))
{}
