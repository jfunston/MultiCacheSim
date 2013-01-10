// Author: Justin Funston
// Date: January 2013
// Email: jfunston@sfu.ca

#ifndef MISC_H
#define MISC_H

#define PAGE_SIZE_4KB

#ifdef PAGE_SIZE_4KB
const unsigned long long PAGE_MASK = 0xFFFFFFFFFFFFF000;
const unsigned int PAGE_SHIFT = 12;
#elif defined PAGE_SIZE_2MB
const unsigned long long PAGE_MASK = 0xFFFFFFFFFFE00000;
const unsigned int PAGE_SHIFT = 21;
#else
#error "Bad PAGE_SIZE"
#endif

enum cacheState {MOD,OWN,EXC,SHA,INV};

struct cacheLine{
   unsigned long long tag;
   cacheState state;
   cacheLine(){tag = 0; state = INV;}
   bool operator<(const cacheLine& rhs) const
   { return tag < rhs.tag; }
   bool operator==(const cacheLine& rhs) const
   { return tag == rhs.tag; }
};

#endif
