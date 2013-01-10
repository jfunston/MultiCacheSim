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

   hits = local_reads = remote_reads = othercache_reads = local_writes 
      = remote_writes = compulsory = 0;

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
