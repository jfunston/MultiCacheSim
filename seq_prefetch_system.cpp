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
int SeqPrefetchSystem::prefetchMiss(unsigned long long address, unsigned int tid)
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
int SeqPrefetchSystem::prefetchHit(unsigned long long address, unsigned int tid)
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

void SeqPrefetchSystem::memAccess(unsigned long long address, char rw, unsigned int tid,
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

SeqPrefetchSystem::SeqPrefetchSystem(unsigned int num_domains, vector<unsigned int> tid_to_domain,
            unsigned int line_size, unsigned int num_lines, unsigned int assoc,
            bool count_compulsory /*=false*/, bool do_addr_trans /*=false*/) : 
            System(num_domains, tid_to_domain, line_size, num_lines, assoc,
               count_compulsory, do_addr_trans)
{
   lastMiss = lastPrefetch = 0; 
}

