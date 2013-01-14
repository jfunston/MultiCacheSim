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

}

System::~System()
{
   for(unsigned int i=0; i<cpus.size(); i++) {
      delete cpus[i];
   }
}

// Keeps track of which NUMA domain each memory page is in,
// using a first-touch policy
void System::updatePageToDomain(unsigned long long address, unsigned int curDomain)
{
   map<unsigned long long, unsigned int>::iterator it;
   unsigned long long page = address & PAGE_MASK;

   it = pageToDomain.find(page);
   if(it == pageToDomain.end()) {
      pageToDomain[page] = curDomain;
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
                           unsigned int local, bool is_prefetch)
{
#ifdef MULTI_CACHE
   unsigned long long page = ((set << SET_SHIFT) | tag) & PAGE_MASK;
#ifdef DEBUG
   map<unsigned long long, unsigned int>::iterator it;
   it = pageList.find(page);
   assert(it != pageList.end());
#endif
   unsigned int domain = pageToDomain[page];
#else
   unsigned int domain = local;
#endif

   if(domain == local && !is_prefetch)
      stats.local_writes++;
   else if(!is_prefetch)
      stats.remote_writes++;
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
   unsigned int domain = pageToDomain[page];

   return (domain == local);
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

cacheState System::processMESI(cacheState remote_state, char rw, 
   bool is_prefetch, bool local_traffic)
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
#ifdef MULTI_CACHE
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
      cpus[remote]->changeState(set, tag, OWN);
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
      cpus[remote]->changeState(set, tag, SHA);
      if(!is_prefetch) {
         stats.othercache_reads++;
      }
   }
#endif

#ifdef DEBUG
   assert(new_state != INV);
#endif

   return new_state;
}
