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

#ifndef MISC_H
#define MISC_H

#include <cstdint>

#define PAGE_SIZE_4KB

#ifdef PAGE_SIZE_4KB
const uint64_t PAGE_MASK = 0xFFFFFFFFFFFFF000;
const uint32_t PAGE_SHIFT = 12;
#elif defined PAGE_SIZE_2MB
const uint64_t PAGE_MASK = 0xFFFFFFFFFFE00000;
const uint32_t PAGE_SHIFT = 21;
#else
#error "Bad PAGE_SIZE"
#endif

enum cacheState {MOD,OWN,EXC,SHA,INV};

struct cacheLine{
   uint64_t tag;
   cacheState state;
   cacheLine(){tag = 0; state = INV;}
   bool operator<(const cacheLine& rhs) const
   { return tag < rhs.tag; }
   bool operator==(const cacheLine& rhs) const
   { return tag == rhs.tag; }
};

#endif
