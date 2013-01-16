// Author: Justin Funston
// Date: January 2013
// Email: jfunston@sfu.ca

#include <cassert>
#include <cmath>
#include <cstdlib>
#include "misc.h"
#include "cache.h"
#include "system.h"

using namespace std;

System::System(unsigned int num_domains, vector<unsigned int> tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            Prefetch* prefetcher, bool count_compulsory /*=false*/, 
            bool do_addr_trans /*=false*/)
{
   assert(num_lines % assoc == 0);

   stats.hits = stats.local_reads = stats.remote_reads = 
      stats.othercache_reads = stats.local_writes = 
      stats.remote_writes = stats.compulsory = 0;

   LINE_MASK = ((unsigned long long) line_size)-1;
   SET_SHIFT = log2(line_size);
   SET_MASK = ((num_lines / assoc) - 1) << SET_SHIFT;
   TAG_MASK = ~(SET_MASK | LINE_MASK);

   nextPage = 0;
  
   countCompulsory = count_compulsory;
   doAddrTrans = do_addr_trans;
   this->tid_to_domain = tid_to_domain;
   this->prefetcher = prefetcher;
}

void System::checkCompulsory(unsigned long long line)
{
   std::set<unsigned long long>::iterator it;

   it = seenLines.find(line);
   if(it == seenLines.end()) {
      stats.compulsory++;
      seenLines.insert(line);
   }
}

unsigned long long System::virtToPhys(unsigned long long address)
{
   map<unsigned long long, unsigned long long>::iterator it;
   unsigned long long virt_page = address & PAGE_MASK;
   unsigned long long phys_page;
   unsigned long long phys_addr = address & (~PAGE_MASK);

   it = virtToPhysMap.find(virt_page);
   if(it != virtToPhysMap.end()) {
      phys_page = it->second;
      phys_addr |= phys_page;
   }
   else {
      phys_page = nextPage << PAGE_SHIFT;
      phys_addr |= phys_page;
      virtToPhysMap.insert(make_pair(virt_page, phys_page));
      //nextPage += rand() % 200 + 5 ;
      ++nextPage;
   }

   return phys_addr;
}

int MultiCacheSystem::checkRemoteStates(unsigned long long set, 
               unsigned long long tag, cacheState& state, unsigned int local)
{
   cacheState curState = INV;
   state = INV;
   int remote = 0;

   for(unsigned int i=0; i<caches.size(); i++) {
      if(i != local) {
         curState = caches[i]->findTag(set, tag);
         if(curState == OWN) {
            state = OWN;
            return i;
         }
         else if(curState == SHA) {
            state = SHA;
            // A cache line in a shared state may be
            // in the owned state in a different cache
            // so don't return i immdiately
            remote = i;
         }
         else if(curState == EXC) {
            state = EXC;
            return i;
         }
         else if(curState == MOD) {
            state = MOD;
            return i;
         }
      }
   }

   return remote;
}

void MultiCacheSystem::setRemoteStates(unsigned long long set, 
               unsigned long long tag, cacheState state, unsigned int local)
{
   for(unsigned int i=0; i<caches.size(); i++) {
      if(i != local) {
         caches[i]->changeState(set, tag, state);
      }
   }
}

// Maintains the statistics for memory write-backs
void MultiCacheSystem::evictTraffic(unsigned long long set, 
               unsigned long long tag, unsigned int local)
{
   unsigned long long page = ((set << SET_SHIFT) | tag) & PAGE_MASK;
#ifdef DEBUG
   map<unsigned long long, unsigned int>::iterator it;
   it = pageList.find(page);
   assert(it != pageList.end());
#endif
   unsigned int domain = pageToDomain[page];

   if(domain == local) {
      stats.local_writes++;
   }
   else {
      stats.remote_writes++;
   }
}

bool MultiCacheSystem::isLocal(unsigned long long address, unsigned int local)
{
   unsigned long long page = address & PAGE_MASK;
#ifdef DEBUG
   map<unsigned long long, unsigned int>::iterator it;
   it = pageList.find(page);
   assert(it != pageList.end());
#endif
   unsigned int domain = pageToDomain[page];

   return (domain == local);
}


cacheState MultiCacheSystem::processMOESI(unsigned long long set,
                  unsigned long long tag, cacheState remote_state, char rw, 
                  bool is_prefetch, bool local_traffic, unsigned int local, 
                  unsigned int remote)
{
   cacheState new_state = INV;

   if(remote_state == INV && rw == 'R') {
      new_state = EXC;
      if(local_traffic && !is_prefetch) {
         stats.local_reads++;
      }
      else if(!is_prefetch) {
         stats.remote_reads++;
      }
   }
   else if(remote_state == INV && rw == 'W') {
      new_state = MOD;
      if(local_traffic && !is_prefetch) {
         stats.local_reads++;
      }
      else if(!is_prefetch) {
         stats.remote_reads++;
      }
   }
   else if(remote_state == SHA && rw == 'R') {
      new_state = SHA;
      if(local_traffic && !is_prefetch) {
         stats.local_reads++;
      }
      else if(!is_prefetch) {
         stats.remote_reads++;
      }
   }
   else if(remote_state == SHA && rw == 'W') {
      new_state = MOD;
      setRemoteStates(set, tag, INV, local);
      if(!is_prefetch) {
         stats.othercache_reads++;
      }
   }
   else if((remote_state == MOD || remote_state == OWN) && rw == 'R') {
      new_state = SHA;
      caches[remote]->changeState(set, tag, OWN);
      if(!is_prefetch) {
         stats.othercache_reads++;
      }
   }
   else if((remote_state == MOD || remote_state == OWN || remote_state == EXC) 
            && rw == 'W') {
      new_state = MOD;
      setRemoteStates(set, tag, INV, local);
      if(!is_prefetch) {
         stats.othercache_reads++;
      }
   }
   else if(remote_state == EXC && rw == 'R') {
      new_state = SHA;
      caches[remote]->changeState(set, tag, SHA);
      if(!is_prefetch) {
         stats.othercache_reads++;
      }
   }

#ifdef DEBUG
   assert(new_state != INV);
#endif

   return new_state;
}

void MultiCacheSystem::memAccess(unsigned long long address, char rw, 
      unsigned int tid, bool is_prefetch)
{
   unsigned long long set, tag;
   unsigned int local;
   bool hit;
   cacheState state;

   if(doAddrTrans) {
      address = virtToPhys(address);
   }

   local = tid_to_domain[tid];
   updatePageToDomain(address, local);

   set = (address & SET_MASK) >> SET_SHIFT;
   tag = address & TAG_MASK;
   state = caches[local]->findTag(set, tag);
   hit = (state != INV);

   if(countCompulsory && !is_prefetch) {
      checkCompulsory(address & LINE_MASK);
   }

   // Handle hits 
   if(rw == 'W' && hit) {  
      caches[local]->changeState(set, tag, MOD);
      setRemoteStates(set, tag, INV, local);
   }

   if(hit) {
      caches[local]->updateLRU(set, tag);
      if(!is_prefetch) {
         stats.hits++;
         prefetcher->prefetchHit(address, tid, this);
      }
      return;
   }

   // Now handle miss cases
   cacheState remote_state;
   cacheState new_state = INV;
   unsigned long long evicted_tag;
   bool writeback;

   unsigned int remote = checkRemoteStates(set, tag, remote_state, local);

   writeback = caches[local]->checkWriteback(set, evicted_tag);
   if(writeback) {
      evictTraffic(set, evicted_tag, local);
   }

   bool local_traffic = isLocal(address, local);

   new_state = processMOESI(set, tag, remote_state, rw, is_prefetch, 
                              local_traffic, local, remote);
   caches[local]->insertLine(set, tag, new_state);
   if(!is_prefetch) {
      prefetcher->prefetchMiss(address, tid, this);
   }
}

// Keeps track of which NUMA domain each memory page is in,
// using a first-touch policy
void MultiCacheSystem::updatePageToDomain(unsigned long long address, 
                                          unsigned int curDomain)
{
   map<unsigned long long, unsigned int>::iterator it;
   unsigned long long page = address & PAGE_MASK;

   it = pageToDomain.find(page);
   if(it == pageToDomain.end()) {
      pageToDomain[page] = curDomain;
   }
}

MultiCacheSystem::MultiCacheSystem(unsigned int num_domains, 
            vector<unsigned int> tid_to_domain, unsigned int line_size, 
            unsigned int num_lines, unsigned int assoc, Prefetch* prefetcher,
            bool count_compulsory /*=false*/, bool do_addr_trans /*=false*/) : 
            System(num_domains, tid_to_domain, line_size, num_lines, assoc,
               prefetcher, count_compulsory, do_addr_trans)
{
   if(assoc > assocCutoff) {
      for(unsigned int i=0; i<num_domains; i++) {
         SetCache* temp = new SetCache(num_lines, assoc);
         caches.push_back(temp);
      }
   }
   else {
      for(unsigned int i=0; i<num_domains; i++) {
         DequeCache* temp = new DequeCache(num_lines, assoc);
         caches.push_back(temp);
      }
   }

   return;
}

MultiCacheSystem::~MultiCacheSystem()
{
   for(unsigned int i=0; i<caches.size(); i++) {
      delete caches[i];
   }
}

void SingleCacheSystem::memAccess(unsigned long long address, char rw, unsigned 
   int tid, bool is_prefetch)
{
   unsigned long long set, tag;
   bool hit;
   cacheState state;

   if(doAddrTrans) {
      address = virtToPhys(address);
   }

   set = (address & SET_MASK) >> SET_SHIFT;
   tag = address & TAG_MASK;
   state = cache->findTag(set, tag);
   hit = (state != INV);

   if(countCompulsory && !is_prefetch) {
      checkCompulsory(address & LINE_MASK);
   }

   // Handle hits 
   if(rw == 'W' && hit) {  
      cache->changeState(set, tag, MOD);
   }

   if(hit) {
      cache->updateLRU(set, tag);
      if(!is_prefetch) {
         stats.hits++;
         prefetcher->prefetchHit(address, tid, this);
      }
      return;
   }

   cacheState new_state = INV;
   unsigned long long evicted_tag;
   bool writeback = cache->checkWriteback(set, evicted_tag);

   if(writeback) {
      stats.local_writes++;
   }

   if(rw == 'R') {
      new_state = EXC;
   }
   else {
      new_state = MOD;
   }

   if(!is_prefetch) {
      stats.local_reads++;
   }

   cache->insertLine(set, tag, new_state);
   if(!is_prefetch) {
      prefetcher->prefetchMiss(address, tid, this);
   }
}

SingleCacheSystem::SingleCacheSystem(unsigned int num_domains, 
            vector<unsigned int> tid_to_domain, unsigned int line_size, 
            unsigned int num_lines, unsigned int assoc, Prefetch* prefetcher,
            bool count_compulsory /*=false*/, bool do_addr_trans /*=false*/) : 
            System(num_domains, tid_to_domain, line_size, num_lines, assoc,
               prefetcher, count_compulsory, do_addr_trans)
{
   if(assoc > assocCutoff) {
         cache = new SetCache(num_lines, assoc);
   }
   else {
         cache = new DequeCache(num_lines, assoc);
   }
}

SingleCacheSystem::~SingleCacheSystem()
{
   delete cache;
}
