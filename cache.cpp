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
#include <iostream>

#include "misc.h"
#include "cache.h"

Cache::Cache(unsigned int num_lines, unsigned int assoc) : maxSetSize(assoc)
{
   assert(num_lines % assoc == 0);
   // The set bits of the address will be used as an index
   // into sets. Each set is a deque containing "assoc" items
   sets.resize(num_lines / assoc);
}

// Given the set and tag, return the cache line's state
// Invalid and "not found" are equivalent
CacheState Cache::findTag(uint64_t set, uint64_t tag) const
{
   for (auto it = sets[set].cbegin(); it != sets[set].cend(); ++it) {
      if (it->tag == tag) {
         return it->state;
      }
   }

   return CacheState::Invalid;
}

// Changes the cache line specificed by "set" and "tag" to "state"
// The cache only saves lines that are not Invalid, so delete if that
// is the new state
void Cache::changeState(uint64_t set, uint64_t tag, CacheState state)
{
   for (auto it = sets[set].begin(); it != sets[set].end(); ++it) {
      if (it->tag == tag) {
         if (state == CacheState::Invalid) {
            sets[set].erase(it);
            break;
         } else {
            it->state = state;
            break;
         }
      }
   }
}

// A complete LRU is mantained for each set, using the ordering
// of the set deque. The end is considered most
// recently used. The specified line must be in the cache, it 
// will be moved to the most-recently used position
void Cache::updateLRU(uint64_t set, uint64_t tag)
{
#ifdef DEBUG
   CacheState foundState = findTag(set, tag);
   assert(foundState != CacheState::Invalid);
#endif

   CacheLine temp;
   auto it = sets[set].begin();
   for(; it != sets[set].end(); ++it) {
      if(it->tag == tag) {
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
bool Cache::checkWriteback(uint64_t set, uint64_t& tag) const
{
   if (sets[set].size() < maxSetSize) {
      // There is room in the set, it does not matter the state of the
      // LRU line
      return false;
   }

   const CacheLine& evict = sets[set].front();
   tag = evict.tag;
   return (evict.state == CacheState::Modified || evict.state == CacheState::Owned);
}

// Insert a new cache line by popping the least recently used line if necessary
// and pushing the new line to the back (most recently used)
void Cache::insertLine(uint64_t set, uint64_t tag, CacheState state)
{
   if (sets[set].size() == maxSetSize) {
      sets[set].pop_front();
   }

   sets[set].emplace_back(tag, state);
}
