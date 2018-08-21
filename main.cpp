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
#include <fstream>
#include <cassert>
#include <sstream>
#include <string>

#include "system.h"

using namespace std;

int main()
{
   // tid_map is used to inform the simulator how
   // thread ids map to NUMA/cache domains. Using
   // the tid as an index gives the NUMA domain.
   unsigned int arr_map[] = {0, 1};
   vector<unsigned int> tid_map(arr_map, arr_map + 
         sizeof(arr_map) / sizeof(unsigned int));
   std::unique_ptr<SeqPrefetch> prefetch = std::make_unique<SeqPrefetch>();
   // The constructor parameters are:
   // the tid_map, the cache line size in bytes,
   // number of cache lines, the associativity,
   // the prefetcher object,
   // whether to count compulsory misses,
   // whether to do virtual to physical translation,
   // and number of caches/domains
   // WARNING: counting compulsory misses doubles execution time
   MultiCacheSystem sys(tid_map, 64, 1024, 64, std::move(prefetch), false, false, 2);
   char rw;
   uint64_t address;
   unsigned long long lines = 0;
   ifstream infile;
   // This code works with the output from the 
   // ManualExamples/pinatrace pin tool
   infile.open("pinatrace.out", ifstream::in);
   assert(infile.is_open());

   while(!infile.eof())
   {
      infile.ignore(256, ':');

      infile >> rw;
      assert(rw == 'R' || rw == 'W');
      AccessType accessType;
      if (rw == 'R') {
         accessType = AccessType::Read;
      } else {
         accessType = AccessType::Write;
      }

      infile >> hex >> address;
      if(address != 0) {
         // By default the pinatrace tool doesn't record the tid,
         // so we make up a tid to stress the MultiCache functionality
         sys.memAccess(address, accessType, lines%2);
      }

      ++lines;
   }

   cout << "Accesses: " << lines << endl;
   cout << "Hits: " << sys.stats.hits << endl;
   cout << "Misses: " << lines - sys.stats.hits << endl;
   cout << "Local reads: " << sys.stats.local_reads << endl;
   cout << "Local writes: " << sys.stats.local_writes << endl;
   cout << "Remote reads: " << sys.stats.remote_reads << endl;
   cout << "Remote writes: " << sys.stats.remote_writes << endl;
   cout << "Other-cache reads: " << sys.stats.othercache_reads << endl;
   //cout << "Compulsory Misses: " << sys.stats.compulsory << endl;
   
   infile.close();

   return 0;
}
