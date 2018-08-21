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

#pragma once

#include <cstdint>

#define PAGE_SIZE_4KB

#ifdef PAGE_SIZE_4KB
constexpr uint64_t pageMask = 0xFFFFFFFFFFFFF000;
constexpr uint32_t pageShift = 12;
#elif defined PAGE_SIZE_2MB
constexpr uint64_t pageMask = 0xFFFFFFFFFFE00000;
constexpr uint32_t pageShift = 21;
#else
#error "Bad PAGE_SIZE"
#endif

enum class CacheState {Modified, Owned, Exclusive, Shared, Invalid};

enum class AccessType {Read, Write, Prefetch};

struct CacheLine{
   uint64_t tag{0};
   CacheState state{CacheState::Invalid};

   CacheLine(uint64_t tag = 0, CacheState state = CacheState::Invalid) : 
               tag(tag), state(state) {}
   bool operator<(const CacheLine& rhs) const
   { return tag < rhs.tag; }
   bool operator==(const CacheLine& rhs) const
   { return tag == rhs.tag; }
};
