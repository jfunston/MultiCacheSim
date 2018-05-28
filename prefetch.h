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

class System;

class Prefetch {
public:
   virtual int prefetchMiss(uint64_t address, unsigned int tid, 
                              System& sys) = 0;
   virtual int prefetchHit(uint64_t address, unsigned int tid,
                              System& sys) = 0;
};

// Modeling AMD's L1 prefetcher, a sequential
// line prefetcher. Primary difference is that
// the real prefetcher has a dynamic prefetch width.
class SeqPrefetch : public Prefetch {
public:
   int prefetchMiss(uint64_t address, unsigned int tid, System& sys) override;
   int prefetchHit(uint64_t address, unsigned int tid, System& sys) override;
private:
   uint64_t lastMiss{0};
   uint64_t lastPrefetch{0};
   static constexpr unsigned int prefetchNum = 3;
};

// A simple adjacent line prefetcher
class AdjPrefetch : public Prefetch {
public:
   int prefetchMiss(uint64_t address, unsigned int tid, System& sys) override;
   int prefetchHit(uint64_t address, unsigned int tid, System& sys) override;
};
