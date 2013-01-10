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

// Called to check for prefetches in the case of a cache miss.
// Models AMD's L1 prefetcher.
int System::prefetchMiss(unsigned long long address, unsigned int tid)
{
   unsigned long long set = (address & SET_MASK) >> SET_SHIFT;
   unsigned long long tag = address & TAG_MASK;
   unsigned long long lastSet = (lastMiss & SET_MASK) >> SET_SHIFT;
   unsigned long long lastTag = lastMiss & TAG_MASK;
   int prefetched = 0;

   if(tag == lastTag && (lastSet+1) == set) {
      for(unsigned int i=0; i < prefetchNum; i++) {
         prefetched++;
         // Call memAccess to resolve the prefetch. The address is 
         // incremented in the set portion of its bits (least
         // significant bits not in the cache line offset portion)
         memAccess(address + ((1 << SET_SHIFT) * (i+1)), 'R', tid, true);
      }
      
      lastPrefetch = address + (1 << SET_SHIFT);
   }

   lastMiss = address;
   return prefetched;
}

// Called to check for prefetches in the case of a cache hit.
// Models AMD's L1 prefetcher.
int System::prefetchHit(unsigned long long address, unsigned int tid)
{
   unsigned long long set = (address & SET_MASK) >> SET_SHIFT;
   unsigned long long tag = address & TAG_MASK;
   unsigned long long lastSet = (lastPrefetch & SET_MASK) >> SET_SHIFT;
   unsigned long long lastTag = lastPrefetch & TAG_MASK;

   if(tag == lastTag && lastSet == set) {
      // Call memAccess to resolve the prefetch. The address is 
      // incremented in the set portion of its bits (least
      // significant bits not in the cache line offset portion)
      memAccess(address + ((1 << SET_SHIFT) * prefetchNum), 'R', tid, true);
      lastPrefetch = lastPrefetch + (1 << SET_SHIFT);
   }

   return 1;
}


System::System(unsigned int num_domains, vector<unsigned int> tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            bool count_compulsory /*=false*/, bool do_addr_trans /*=false*/)
{
   assert(num_lines % assoc == 0);

   if(assoc > 64) {
      for(unsigned int i=0; i<num_domains; i++) {
         SetCache* temp = new SetCache(num_lines, assoc);
         cpus.push_back(temp);
      }
   }
   else {
      for(unsigned int i=0; i<num_domains; i++) {
         DequeCache* temp = new DequeCache(num_lines, assoc);
         cpus.push_back(temp);
      }
   }

   hits = local_reads = remote_reads = othercache_reads = local_writes = remote_writes = 0;
   lastMiss = lastPrefetch = cycles = compulsory = 0;

   LINE_MASK = ((unsigned long long) line_size)-1;
   SET_SHIFT = log2(line_size);
   SET_MASK = ((num_lines / assoc) - 1) << SET_SHIFT;
   TAG_MASK = ~(SET_MASK | LINE_MASK);

   nextPage = 0;
  
   countCompulsory = count_compulsory;
   doAddrTrans = do_addr_trans;
   this->tid_to_domain = tid_to_domain;
}

System::~System()
{
   for(unsigned int i=0; i<cpus.size(); i++) {
      delete cpus[i];
   }
}

// Keeps track of which NUMA domain each memory page is in,
// using a first-touch policy
void System::updatePageList(unsigned long long address, unsigned int curDomain)
{
   map<unsigned long long, unsigned long long>::iterator it;
   unsigned long long page = address & PAGE_MASK;

   it = pageList.find(page);
   if(it == pageList.end()) {
      pageList[page] = curDomain;
   }
}

void System::setRemoteStates(unsigned long long set, unsigned long long tag,
                        cacheState state, unsigned int local)
{
   for(unsigned int i=0; i<cpus.size(); i++) {
      if(i != local) {
         cpus[i]->changeState(set, tag, state);
      }
   }
}

// Maintains the statistics for memory write-backs
void System::evictTraffic(unsigned long long set, unsigned long long tag,
                           unsigned int local, bool isPrefetch)
{
#ifdef MULTI_CACHE
   unsigned long long page = ((set << SET_SHIFT) | tag) & PAGE_MASK;
#ifdef DEBUG
   map<unsigned long long, unsigned int>::iterator it;
   it = pageList.find(page);
   assert(it != pageList.end());
#endif
   unsigned int domain = pageList[page];
#else
   unsigned int domain = local;
#endif

   if(domain == local && !isPrefetch)
      local_writes++;
   else if(!isPrefetch)
      remote_writes++;
}

bool System::isLocal(unsigned long long address,
                     unsigned int local)
{
   unsigned long long page = address & PAGE_MASK;
#ifdef DEBUG
   map<unsigned long long, unsigned int>::iterator it;
   it = pageList.find(page);
   assert(it != pageList.end());
#endif
   unsigned int domain = pageList[page];
   if(domain == local) {
      return true;
   }
   else {
      return false;
   }
}

void System::checkCompulsory(unsigned long long line)
{
   std::set<unsigned long long>::iterator it;

   it = seenLines.find(line);
   if(it == seenLines.end()) {
      ++compulsory;
      seenLines.insert(line);
   }
}

int System::checkRemoteStates(unsigned long long set, unsigned long long tag, 
                              cacheState& state, unsigned int local)
{
   cacheState curState = INV;
   state = INV;
   int remote = 0;

   for(unsigned int i=0; i<cpus.size(); i++) {
      if(i != local) {
         curState = cpus[i]->findTag(set, tag);
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


void System::memAccess(unsigned long long address, char rw, unsigned int tid,
                        bool isPrefetch)
{
   unsigned long long set, tag;
   bool hit;
   cacheState state;

// Optimizations for single cache simulation
#ifdef MULTI_CACHE 
   unsigned int local = tid_to_domain[tid];
   updatePageList(address, local);
#else
   unsigned int local = 0;
#endif

// Address translation and MULTI_CACHE are
// currently mutually exclusive
#ifndef MULTI_CACHE
   if(doAddrTrans) {
      address = virtToPhys(address);
   }
#endif

   set = (address & SET_MASK) >> SET_SHIFT;
   tag = address & TAG_MASK;
   state = cpus[local]->findTag(set, tag);
   hit = (state != INV);

   if(countCompulsory && !isPrefetch) {
      checkCompulsory(address & LINE_MASK);
   }

   // Handle hits 
   if(rw == 'W' && hit) {  
      cpus[local]->changeState(set, tag, MOD);
#ifdef MULTI_CACHE
      setRemoteStates(set, tag, INV, local);
#endif
   }

   if(hit) {
      cpus[local]->updateLRU(set, tag);
      if(!isPrefetch) {
         hits++;
         prefetchHit(address, tid);
      }
      return;
   }

   if(!isPrefetch) {
      prefetchMiss(address, tid);
   }

   // Now handle miss cases
   cacheState remote_state;
#ifdef MULTI_CACHE
   remote = checkRemoteStates(set, tag, remote_state, local);
#else
   remote_state = INV;
#endif
   cacheState new_state = INV;

   unsigned long long evicted_tag;
   bool writeback = cpus[local]->checkWriteback(set, evicted_tag);
   if(writeback)
      evictTraffic(set, evicted_tag, local, isPrefetch);

#ifdef MULTI_CACHE 
   bool local_traffic = isLocal(address, local);
#else
   bool local_traffic = true;
#endif

   if(remote_state == INV && rw == 'R') {
      new_state = EXC;
      if(local_traffic && !isPrefetch) {
         local_reads++;
      }
      else if(!isPrefetch) {
         remote_reads++;
      }
   }
   else if(remote_state == INV && rw == 'W') {
      new_state = MOD;
      if(local_traffic && !isPrefetch) {
         local_reads++;
      }
      else if(!isPrefetch) {
         remote_reads++;
      }
   }
#ifdef MULTI_CACHE
   else if(remote_state == SHA && rw == 'R') {
      new_state = SHA;
      if(local_traffic && !isPrefetch) {
         local_reads++;
      }
      else if(!isPrefetch) {
         remote_reads++;
      }
   }
   else if(remote_state == SHA && rw == 'W') {
      new_state = MOD;
      setRemoteStates(set, tag, INV, local);
      if(!isPrefetch) {
         othercache_reads++;
      }
   }
   else if((remote_state == MOD || remote_state == OWN) && rw == 'R') {
      new_state = SHA;
      cpus[remote]->changeState(set, tag, OWN);
      if(!isPrefetch) {
         othercache_reads++;
      }
   }
   else if((remote_state == MOD || remote_state == OWN || remote_state == EXC) 
            && rw == 'W') {
      new_state = MOD;
      setRemoteStates(set, tag, INV, local);
      if(!isPrefetch) {
         othercache_reads++;
      }
   }
   else if(remote_state == EXC && rw == 'R') {
      new_state = SHA;
      cpus[remote]->changeState(set, tag, SHA);
      if(!isPrefetch) {
         othercache_reads++;
      }
   }
#endif

#ifdef DEBUG
   assert(new_state != INV);
#endif

   cpus[local]->insertLine(set, tag, new_state);
}

unsigned long long System::virtToPhys(unsigned long long address)
{
   map<unsigned long long, unsigned long long>::iterator it;
   unsigned long long virt_page = address & PAGE_MASK;
   unsigned long long phys_page;
   unsigned long long phys_addr = address & (~PAGE_MASK);

   it = pageList.find(virt_page);
   if(it != pageList.end()) {
      phys_page = it->second;
      phys_addr |= phys_page;
   }
   else {
      phys_page = nextPage << PAGE_SHIFT;
      phys_addr |= phys_page;
      pageList.insert(make_pair(virt_page, phys_page));
      //nextPage += rand() % 200 + 5 ;
      ++nextPage;
   }

   return phys_addr;
}
