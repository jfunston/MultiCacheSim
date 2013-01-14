// Author: Justin Funston
// Date: January 2013
// Email: jfunston@sfu.ca

#include "prefetch.h"
#include "system.h"

int AdjPrefetch::prefetchMiss(unsigned long long address, unsigned int tid,
      System* sys)
{
   sys->memAccess(address + (1 << sys->SET_SHIFT), 'R', tid, true);
   return 1;
}

// Called to check for prefetches in the case of a cache miss.
int SeqPrefetch::prefetchMiss(unsigned long long address, unsigned int tid,
      System* sys)
{
   unsigned long long set = (address & sys->SET_MASK) >> sys->SET_SHIFT;
   unsigned long long tag = address & sys->TAG_MASK;
   unsigned long long lastSet = (lastMiss & sys->SET_MASK) >> sys->SET_SHIFT;
   unsigned long long lastTag = lastMiss & sys->TAG_MASK;
   int prefetched = 0;

   if(tag == lastTag && (lastSet+1) == set) {
      for(unsigned int i=0; i < prefetchNum; i++) {
         prefetched++;
         // Call memAccess to resolve the prefetch. The address is 
         // incremented in the set portion of its bits (least
         // significant bits not in the cache line offset portion)
         sys->memAccess(address + ((1 << sys->SET_SHIFT) * (i+1)), 'R', tid, true);
      }
      
      lastPrefetch = address + (1 << sys->SET_SHIFT);
   }

   lastMiss = address;
   return prefetched;
}

int AdjPrefetch::prefetchHit(unsigned long long address, unsigned int tid,
      System* sys)
{
   sys->memAccess(address + (1 << sys->SET_SHIFT), 'R', tid, true);
   return 1;
}

// Called to check for prefetches in the case of a cache hit.
int SeqPrefetch::prefetchHit(unsigned long long address, unsigned int tid,
      System* sys)
{
   unsigned long long set = (address & sys->SET_MASK) >> sys->SET_SHIFT;
   unsigned long long tag = address & sys->TAG_MASK;
   unsigned long long lastSet = (lastPrefetch & sys->SET_MASK) >> sys->SET_SHIFT;
   unsigned long long lastTag = lastPrefetch & sys->TAG_MASK;

   if(tag == lastTag && lastSet == set) {
      // Call memAccess to resolve the prefetch. The address is 
      // incremented in the set portion of its bits (least
      // significant bits not in the cache line offset portion)
      sys->memAccess(address + ((1 << sys->SET_SHIFT) * prefetchNum), 
                        'R', tid, true);
      lastPrefetch = lastPrefetch + (1 << sys->SET_SHIFT);
   }

   return 1;
}

