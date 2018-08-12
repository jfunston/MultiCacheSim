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

#include <iostream>

#include "system.h"

#define CATCH_CONFIG_MAIN
#include "tests/catch.hpp"

TEST_CASE("Single cache tests", "[cache]") {
   unsigned int cache_line_size = 64;
   unsigned int cache_lines = 128;
   unsigned int way_count = 4;
   bool compulsory = false;

   // Address layout we expect:
   // Offset mask: 0x000000000000003F
   // Set mask:    0x00000000000007C0


   std::unique_ptr<System> sys = std::make_unique<SingleCacheSystem>(cache_line_size, 
                              cache_lines, way_count, 
                              nullptr, //no prefetch
                              compulsory);

   SECTION("Simple miss-hit sequence") {
      REQUIRE(sys->stats.accesses == 0);
      REQUIRE(sys->stats.hits == 0);
      REQUIRE(sys->stats.local_reads == 0);
      REQUIRE(sys->stats.local_writes == 0);

      sys->memAccess(0x0000000000000000ULL, 'w', 0);
      REQUIRE(sys->stats.accesses == 1);
      REQUIRE(sys->stats.hits == 0);
      REQUIRE(sys->stats.local_reads == 1);

      sys->memAccess(0x0000000000000000ULL, 'r', 0);
      REQUIRE(sys->stats.accesses == 2);
      REQUIRE(sys->stats.hits == 1);
      REQUIRE(sys->stats.local_reads == 1);

      SECTION("Set fill") {
         sys->memAccess(0x0001000000000000ULL, 'w', 0);
         sys->memAccess(0x0002000000000000ULL, 'w', 0);
         sys->memAccess(0x0003000000000000ULL, 'w', 0);

         REQUIRE(sys->stats.accesses == 5);
         REQUIRE(sys->stats.hits == 1);
         REQUIRE(sys->stats.local_reads == 4);

         SECTION("Other sets fill") {
            uint64_t tag = 0x0001000000000000ULL;
            // 128 / 4 = 32 total sets
            for (uint64_t i = 1; i < 32; ++i) {
               uint64_t addr = tag | (i << 6);
               sys->memAccess(addr, 'w', 0);
            }

            REQUIRE(sys->stats.accesses == 36);
            REQUIRE(sys->stats.hits == 1);
            REQUIRE(sys->stats.local_reads == 35);

            // Original set should be unaffect and these should be hits
            sys->memAccess(0x0000000000000000ULL, 'r', 0);
            sys->memAccess(0x0001000000000000ULL, 'r', 0);
            sys->memAccess(0x0002000000000000ULL, 'r', 0);
            sys->memAccess(0x0003000000000000ULL, 'r', 0);
            REQUIRE(sys->stats.hits == 5);
         }

         SECTION("Set hits") {
            sys->memAccess(0x0001000000000000ULL, 'r', 0);
            sys->memAccess(0x0002000000000000ULL, 'r', 0);
            sys->memAccess(0x0003000000000000ULL, 'r', 0);

            REQUIRE(sys->stats.accesses == 8);
            REQUIRE(sys->stats.hits == 4);
            REQUIRE(sys->stats.local_reads == 4);
         }

         SECTION("Evict") {
            sys->memAccess(0x0004000000000000ULL, 'w', 0);
            REQUIRE(sys->stats.local_reads == 5);
            REQUIRE(sys->stats.hits == 1);

            sys->memAccess(0x0000000000000000ULL, 'r', 0);
            REQUIRE(sys->stats.local_reads == 6);
            REQUIRE(sys->stats.hits == 1);
         }

         SECTION("Evict LRU") {
            sys->memAccess(0x0000000000000000ULL, 'r', 0);
            REQUIRE(sys->stats.local_reads == 4);
            REQUIRE(sys->stats.hits == 2);

            sys->memAccess(0x0004000000000000ULL, 'w', 0);
            REQUIRE(sys->stats.local_reads == 5);
            REQUIRE(sys->stats.hits == 2);

            sys->memAccess(0x0000000000000000ULL, 'r', 0);
            REQUIRE(sys->stats.local_reads == 5);
            REQUIRE(sys->stats.hits == 3);
         }
      }
   }
}
