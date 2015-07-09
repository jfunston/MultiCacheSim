/*
Copyright (c) 2015 Justin Funston

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
#include <iostream>
#include <utility>
#include "misc.h"
#include "cache.h"

SetCache::SetCache(unsigned int num_lines, unsigned int assoc)
{
   assert(num_lines % assoc == 0);
   // The set bits of the address will be used as an index
   // into sets. Each set is a set containing "assoc" items
   sets.resize(num_lines / assoc);
   lruLists.resize(num_lines / assoc);
   lruMaps.resize(num_lines / assoc);
   for(unsigned int i=0; i < sets.size(); i++) {
      for(unsigned int j=0; j < assoc; ++j) {
         cacheLine temp;
         temp.tag = j;
         temp.state = INV;
         sets[i].insert(temp);
         lruLists[i].push_front(j);
         lruMaps[i].insert(make_pair(j, lruLists[i].begin()));
      }
   }
}

/* FIXME invalid vs not found */
// Given the set and tag, return the cache lines state
cacheState SetCache::findTag(uint64_t set, 
                              uint64_t tag) const
{
   cacheLine temp;
   temp.tag = tag;
   std::set<cacheLine>::const_iterator it = sets[set].find(temp);

   if(it != sets[set].end()) {
      return it->state;
   }

   return INV;
}

/* FIXME invalid vs not found */
// Changes the cache line specificed by "set" and "tag" to "state"
void SetCache::changeState(uint64_t set, uint64_t tag, 
                              cacheState state)
{
   cacheLine temp;
   temp.tag = tag;
   std::set<cacheLine>::const_iterator it = sets[set].find(temp);

   if(it != sets[set].end()) {
      cacheLine *target;
      target = (cacheLine*)&*it;
      target->state = state;
   }
}

// A complete LRU is mantained for each set, using a separate
// list and map. The front of the list is considered most recently used.
void SetCache::updateLRU(uint64_t set, uint64_t tag)
{
   std::unordered_map<uint64_t, 
      std::list<uint64_t>::iterator>::iterator map_it;
   std::list<uint64_t>::iterator it;
   uint64_t temp;

   map_it = lruMaps[set].find(tag);

   #ifdef DEBUG
   assert(map_it != lruMaps[set].end());
   cacheState foundState = findTag(set, tag);
   assert(foundState != INV);
   #endif

   it = map_it->second;

   #ifdef DEBUG
   assert(it != lruLists[set].end());
   #endif

   temp = *it;

   lruLists[set].erase(it);
   lruLists[set].push_front(temp);

   lruMaps[set].erase(map_it);
   lruMaps[set].insert(make_pair(tag, lruLists[set].begin()));
}

// Called if a new cache line is to be inserted. Checks if
// the least recently used line needs to be written back to
// main memory.
bool SetCache::checkWriteback(uint64_t set, 
                                 uint64_t& tag) const
{
   cacheLine evict, temp;
   tag = lruLists[set].back();
   temp.tag = tag;
   evict = *sets[set].find(temp);

   return (evict.state == MOD || evict.state == OWN);
}

// FIXME: invalid vs not found
// Insert a new cache line by popping the least recently used line
// and pushing the new line to the back (most recently used)
void SetCache::insertLine(uint64_t set, uint64_t tag, 
                           cacheState state)
{
   uint64_t to_evict = lruLists[set].back();
   cacheLine newLine, temp;
   newLine.tag = tag;
   newLine.state = state;
   temp.tag = to_evict;

   sets[set].erase(temp);
   sets[set].insert(newLine);
   
   lruMaps[set].erase(to_evict);
   lruLists[set].pop_back();
   lruLists[set].push_front(tag);
   lruMaps[set].insert(make_pair(tag, lruLists[set].begin()));
}

DequeCache::DequeCache(unsigned int num_lines, unsigned int assoc)
{
   assert(num_lines % assoc == 0);
   // The set bits of the address will be used as an index
   // into sets. Each set is a deque containing "assoc" items
   sets.resize(num_lines / assoc);
   for(unsigned int i=0; i < sets.size(); i++) {
      sets[i].resize(assoc);
   }
}

// Given the set and tag, return the cache lines state
// INVALID and "not found" are equivalent
cacheState DequeCache::findTag(uint64_t set, 
                                 uint64_t tag) const
{
   std::deque<cacheLine>::const_iterator it = sets[set].begin();

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
void DequeCache::changeState(uint64_t set, uint64_t tag, 
                              cacheState state)
{
   std::deque<cacheLine>::iterator it = sets[set].begin();

   for(; it != sets[set].end(); ++it)
   {
      // The cache may hold many invalid entries of a given line
      // and 1 or 0 valid entries. Use the valid entry
      if(it->tag == tag && it->state != INV)
         it->state = state;
   } 
}

// A complete LRU is mantained for each set, using the ordering
// of the set deque. The end is considered most
// recently used.
void DequeCache::updateLRU(uint64_t set, uint64_t tag)
{
   std::deque<cacheLine>::iterator it = sets[set].begin();
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
bool DequeCache::checkWriteback(uint64_t set, 
                                 uint64_t& tag) const
{
   cacheLine evict = sets[set].front();
   tag = evict.tag;

   return (evict.state == MOD || evict.state == OWN);
}

// Insert a new cache line by popping the least recently used line
// and pushing the new line to the back (most recently used)
void DequeCache::insertLine(uint64_t set, uint64_t tag, 
                              cacheState state)
{
   cacheLine newLine;
   newLine.tag = tag;
   newLine.state = state;

   sets[set].pop_front();
   sets[set].push_back(newLine);
}

