/*
Copyright (c) 2013 Justin Funston

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

#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <set>
#include <list>
#include <deque>
#include <set>
#include <unordered_map>
#include "misc.h"

class Cache{
public:
   virtual ~Cache(){}
   virtual cacheState findTag(unsigned long long set, 
                                 unsigned long long tag) const = 0;
   virtual void changeState(unsigned long long set, 
                              unsigned long long tag, cacheState state) = 0;
   virtual void updateLRU(unsigned long long set, unsigned long long tag) = 0;
   virtual bool checkWriteback(unsigned long long set, 
      unsigned long long& tag) const = 0;
   virtual void insertLine(unsigned long long set, 
                              unsigned long long tag, cacheState state) = 0;
};

class SetCache : public Cache{
   std::vector<std::set<cacheLine> > sets;
   std::vector<std::list<unsigned long long> > lruLists;
   std::vector<std::unordered_map<unsigned long long, 
      std::list<unsigned long long>::iterator> > lruMaps;
public:
   SetCache(unsigned int num_lines, unsigned int assoc);
   cacheState findTag(unsigned long long set, unsigned long long tag) const;
   void changeState(unsigned long long set, unsigned long long tag, 
                     cacheState state);
   void updateLRU(unsigned long long set, unsigned long long tag);
   bool checkWriteback(unsigned long long set, unsigned long long& tag) const;
   void insertLine(unsigned long long set, unsigned long long tag, 
                     cacheState state);
};

class DequeCache : public Cache{
   std::vector<std::deque<cacheLine> > sets;
public:
   DequeCache(unsigned int num_lines, unsigned int assoc);
   cacheState findTag(unsigned long long set, unsigned long long tag) const;
   void changeState(unsigned long long set, unsigned long long tag, 
                    cacheState state);
   void updateLRU(unsigned long long set, unsigned long long tag);
   bool checkWriteback(unsigned long long set, unsigned long long& tag) const;
   void insertLine(unsigned long long set, unsigned long long tag, 
                     cacheState state);
};

#endif

