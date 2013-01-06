// Author: Justin Funston
// Date: November 2011
// Email: jfunston@sfu.ca

#include "cache.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace std;

Cache::Cache(unsigned int num_lines, unsigned int assoc)
{
   assert(num_lines % assoc == 0);
   // The set bits of the address will be used as an index
   // into sets. Each set is a list containing "assoc" items
   sets.resize(num_lines / assoc);
   for(unsigned int i=0; i < sets.size(); i++)
   {
      sets[i].resize(assoc);
   }
}

// Given the set and tag, return the cache lines state
// INVALID and "not found" are equivalent
cacheState Cache::findTag(unsigned long long set, unsigned long long tag) const
{
   list<cacheLine>::const_iterator it = sets[set].begin();

   for(; it != sets[set].end(); ++it)
   {
      // The cache may hold many invalid entries of a given line
      // and 1 or 0 valid entries. Return the valid entry if it
      // exists.
      if(it->tag == tag && it->state != INV)
         return it->state;
   }

   return INV;
}

// Changes the cache line specificed by "set" and "tag" to "state"
void Cache::changeState(unsigned long long set, unsigned long long tag, cacheState state)
{
   list<cacheLine>::iterator it = sets[set].begin();

   for(; it != sets[set].end(); ++it)
   {
      // The cache may hold many invalid entries of a given line
      // and 1 or 0 valid entries. Use the valid entry
      if(it->tag == tag && it->state != INV)
         it->state = state;
   } 
}

// A complete LRU is mantained for each set, using the ordering
// of the set list. The end of the list is considered most
// recently used.
void Cache::updateLRU(unsigned long long set, unsigned long long tag)
{
   list<cacheLine>::iterator it = sets[set].begin();
   cacheLine temp;

   #ifdef DEBUG
   cacheState foundState = findTag(set, tag);
   assert(foundState != INV);
   #endif

   for(; it != sets[set].end(); ++it)
   {
      // The cache may hold many invalid entries of a given line
      // and 1 or 0 valid entries. Use the valid entry.
      if(it->tag == tag && it->state != INV)
      {
         temp = *it;
         break;
      }
   } 

   sets[set].erase(it);
   sets[set].push_back(temp);
}

// Called if a new cache line is to be inserted. Checks if
// the least recently used line needs to be written back to
// main memory.
bool Cache::checkWriteback(unsigned long long set, unsigned long long& tag) const
{
   cacheLine evict = sets[set].front();
   tag = evict.tag;
   if(evict.state == MOD || evict.state == OWN)
      return true;
   return false;
}

// Insert a new cache line by popping the least recently used line
// and pushing the new line to the back (most recently used)
void Cache::insertLine(unsigned long long set, unsigned long long tag, cacheState state)
{
   cacheLine newLine;
   newLine.tag = tag;
   newLine.state = state;

   sets[set].pop_front();
   sets[set].push_back(newLine);
}

// Called to check for prefetches in the case of a cache miss.
// Models AMD's L1 prefetcher.
int System::prefetchMiss(unsigned long long address, unsigned int tid)
{
   unsigned long long set = (address & SET_MASK) >> SET_SHIFT;
   unsigned long long tag = address & TAG_MASK;
   unsigned long long lastSet = (lastMiss & SET_MASK) >> SET_SHIFT;
   unsigned long long lastTag = lastMiss & TAG_MASK;
   int prefetched = 0;

   if(tag == lastTag && (lastSet+1) == set)
   {
      for(unsigned int i=0; i < prefetchNum; i++)
      {
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

   if(tag == lastTag && lastSet == set)
   {
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
            bool count_compulsory /*=false*/)
{
   assert(num_lines % assoc == 0);

   Cache temp(num_lines, assoc);
   for(unsigned int i=0; i<num_domains; i++) {
      cpus.push_back(temp);
   }

   hits = local_reads = remote_reads = othercache_reads = local_writes = remote_writes = 0;
   lastMiss = lastPrefetch = cycles = compulsory = 0;

   LINE_MASK = ((unsigned long long) line_size)-1;
   SET_SHIFT = log2(line_size);
   SET_MASK = ((num_lines / assoc) - 1) << SET_SHIFT;
   TAG_MASK = ~(SET_MASK | LINE_MASK);

   // nextFreePage = 0;
  
   countCompulsory = count_compulsory;
   this->tid_to_domain = tid_to_domain;
}

// Translates virtual addresses to physical addresses
// Keeps track of which NUMA domain each memory page is in,
// using a first-touch policy
void System::updatePageList(unsigned long long address, unsigned int curDomain)
{
   map<unsigned long long, unsigned int>::iterator it;
   unsigned long long page = address & PAGE_MASK;

   it = pageList.find(page);
   if(it == pageList.end())
   {
      pageList[page] = curDomain;
   }
}

void System::setRemoteStates(unsigned long long set, unsigned long long tag,
                        cacheState state, unsigned int local)
{
   for(unsigned int i=0; i<cpus.size(); i++)
   {
      if(i != local)
         cpus[i].changeState(set, tag, state);
   }
}

// Maintains the statistics for memory write-backs
void System::evictTraffic(unsigned long long set, unsigned long long tag,
                           unsigned int local, bool isPrefetch)
{
   unsigned long long page = ((set << SET_SHIFT) | tag) & PAGE_MASK;
   #ifdef DEBUG
   map<unsigned long long, unsigned int>::iterator it;
   it = pageList.find(page);
   assert(it != pageList.end());
   #endif
   unsigned int domain = pageList[page];
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
   if(domain == local)
      return true;
   else
      return false;
}

void System::checkCompulsory(unsigned long long line)
{
   set<unsigned long long>::iterator it;

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

   for(unsigned int i=0; i<cpus.size(); i++)
   {
      if(i != local)
      {
         curState = cpus[i].findTag(set, tag);
         if(curState == OWN)
         {
            state = OWN;
            return i;
         }
         else if(curState == SHA)
         {
            state = SHA;
            // A cache line in a shared state may be
            // in the owned state in a different cache
            // so don't return i immdiately
            remote = i;
         }
         else if(curState == EXC)
         {
            state = EXC;
            return i;
         }
         else if(curState == MOD)
         {
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
   unsigned int local = tid_to_domain[tid];
   updatePageList(address, local);

   unsigned long long set = (address & SET_MASK) >> SET_SHIFT;
   unsigned long long tag = address & TAG_MASK;
   cacheState state = cpus[local].findTag(set, tag);
   bool hit = (state != INV);

   if(countCompulsory && !isPrefetch) {
      checkCompulsory(address & LINE_MASK);
   }

   // Handle hits 
   if(rw == 'W' && hit) {  
      cpus[local].changeState(set, tag, MOD);
// Optimizations for single cache simulation
#ifdef MULTI_CACHE
      setRemoteStates(set, tag, INV, local);
#endif
   }

   if(hit) {
      cpus[local].updateLRU(set, tag);
      if(!isPrefetch) {
         hits++;
         prefetchHit(address, tid);
      }
      return;
   }

   if(!isPrefetch)
      prefetchMiss(address, tid);

   // Now handle miss cases
   cacheState remote_state;
#ifdef MULTI_CACHE
   remote = checkRemoteStates(set, tag, remote_state, local);
#else
   remote_state = INV;
#endif
   cacheState new_state = INV;

   unsigned long long evicted_tag;
   bool writeback = cpus[local].checkWriteback(set, evicted_tag);
   if(writeback)
      evictTraffic(set, evicted_tag, local, isPrefetch);

#ifdef MULTI_CACHE 
   bool local_traffic = isLocal(address, local);
#else
   bool local_traffic = true;
#endif

   if(remote_state == INV && rw == 'R')
   {
      new_state = EXC;
      if(local_traffic && !isPrefetch)
         local_reads++;
      else if(!isPrefetch)
         remote_reads++;
   }
   else if(remote_state == INV && rw == 'W')
   {
      new_state = MOD;
      if(local_traffic && !isPrefetch)
         local_reads++;
      else if(!isPrefetch)
         remote_reads++;
   }
#ifdef MULTI_CACHE
   else if(remote_state == SHA && rw == 'R')
   {
      new_state = SHA;
      if(local_traffic && !isPrefetch)
         local_reads++;
      else if(!isPrefetch)
         remote_reads++;
   }
   else if(remote_state == SHA && rw == 'W')
   {
      new_state = MOD;
      setRemoteStates(set, tag, INV, local);
      if(!isPrefetch)
         othercache_reads++;
   }
   else if((remote_state == MOD || remote_state == OWN) && rw == 'R')
   {
      new_state = SHA;
      cpus[remote].changeState(set, tag, OWN);
      if(!isPrefetch)
         othercache_reads++;
   }
   else if((remote_state == MOD || remote_state == OWN || remote_state == EXC) 
            && rw == 'W')
   {
      new_state = MOD;
      setRemoteStates(set, tag, INV, local);
      if(!isPrefetch)
         othercache_reads++;
   }
   else if(remote_state == EXC && rw == 'R')
   {
      new_state = SHA;
      cpus[remote].changeState(set, tag, SHA);
      if(!isPrefetch)
         othercache_reads++;
   }
#endif

   #ifdef DEBUG
   assert(new_state != INV);
   #endif

   cpus[local].insertLine(set, tag, new_state);
}

