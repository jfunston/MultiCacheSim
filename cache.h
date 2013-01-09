// Author: Justin Funston
// Date: November 2011
// Modified: January 2013
// Email: jfunston@sfu.ca

#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <set>
#include <list>
#include <deque>
#include <set>
#include <unordered_map>
#include "misc.h"

using namespace std;

class Cache{
public:
   //Cache(unsigned int num_lines, unsigned int assoc) = 0;
   virtual ~Cache(){};
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
   vector<set<cacheLine> > sets;
   vector<list<unsigned long long> > lruLists;
   vector<unordered_map<unsigned long long, 
      list<unsigned long long>::iterator> > lruMaps;
public:
   SetCache(unsigned int num_lines, unsigned int assoc);
   cacheState findTag(unsigned long long set, unsigned long long tag) const;
   void changeState(unsigned long long set, unsigned long long tag, cacheState state);
   void updateLRU(unsigned long long set, unsigned long long tag);
   bool checkWriteback(unsigned long long set, unsigned long long& tag) const;
   void insertLine(unsigned long long set, unsigned long long tag, cacheState state);
};

class DequeCache : public Cache{
   vector<deque<cacheLine> > sets;
public:
   DequeCache(unsigned int num_lines, unsigned int assoc);
   cacheState findTag(unsigned long long set, unsigned long long tag) const;
   void changeState(unsigned long long set, unsigned long long tag, cacheState state);
   void updateLRU(unsigned long long set, unsigned long long tag);
   bool checkWriteback(unsigned long long set, unsigned long long& tag) const;
   void insertLine(unsigned long long set, unsigned long long tag, cacheState state);
};

#endif
