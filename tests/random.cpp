/*
Copyright (c) 2018 Justin Funston

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
#include <string>
#include <random>
#include <chrono>

#include "system.h"

using namespace std;

void usage() {
   cout << "Usage: ./random <# cache lines> <associativity ways> "
        << "<prefetecher: 'none'|'adjacent'|'sequential'>"
        << " <compulsory misses 'y'|'n'> <# caches/domains> <# threads>"
        << " <# iterations> <distribution 'uniform'|'normal'> <distribution range>" << endl;
}

struct AccessData {
   uint64_t address;
   unsigned int tid;
   AccessType accessType;
};

int main(int argc, char* argv[]) {
   if (argc != 10) {
      usage();
      return -1;
   }

   unsigned int cache_line_size = 64;
   unsigned int cache_lines = stoi(argv[1]);
   unsigned int way_count = stoi(argv[2]);
   string prefetcher_choice(argv[3]);
   string compulsory_choice(argv[4]);
   unsigned int num_caches = stoi(argv[5]);
   unsigned int num_threads = stoi(argv[6]);
   unsigned int iterations = stoi(argv[7]);
   string distribution_choice(argv[8]);
   uint64_t range = stoull(argv[9]);

   // tid_map is used to inform the simulator how
   // thread ids map to NUMA/cache domains. Using
   // the tid as an index gives the NUMA domain.
   vector<unsigned int> tid_map(num_threads);
   for (unsigned int i=0; i<num_threads; ++i) {
      tid_map[i] = i % num_caches;
   }

   bool compulsory;
   if (compulsory_choice == "y") {
      compulsory = true;
   } else if (compulsory_choice == "n") {
      compulsory = false;
   } else {
      usage();
      return -1;
   }

   if (distribution_choice != "uniform" && distribution_choice != "normal") {
      usage();
      return -1;
   }

   std::unique_ptr<Prefetch> prefetch;
   if (prefetcher_choice == "none") {
      prefetch = nullptr;
   } else if (prefetcher_choice == "adjacent") {
      prefetch = std::make_unique<AdjPrefetch>();
   } else if (prefetcher_choice == "sequential") {
      prefetch = std::make_unique<SeqPrefetch>();
   } else {
      usage();
      return -1;
   }

   unique_ptr<System> sys;
   if (num_caches == 1) {
      sys = make_unique<SingleCacheSystem>(cache_line_size, cache_lines, way_count, 
                           std::move(prefetch), 
                           compulsory, false);
   } else {
      sys = make_unique<MultiCacheSystem>(tid_map, cache_line_size, cache_lines, way_count, 
                           std::move(prefetch), 
                           compulsory, false, num_caches);
   }

   array<AccessData, 2000> access_buffer;
   default_random_engine engine(0);
   uniform_int_distribution<unsigned int> tid_generator(0, num_threads);
   uniform_int_distribution<unsigned int> rw_generator(0, 1);
   uniform_int_distribution<uint64_t> addr_uniform(0, range);
   normal_distribution<double> addr_normal(1000000000.0, (double)range);

   chrono::duration<double> run_time;

   for (unsigned int i=0; i<iterations; ++i) {
      for (int j=0; j<2000; ++j) {
         access_buffer[j].accessType = rw_generator(engine) == 0 ? 
                                          AccessType::Read : 
                                          AccessType::Write;
         access_buffer[j].tid = tid_generator(engine);
            if (distribution_choice == "uniform") {
               access_buffer[j].address = addr_uniform(engine);
            } else {
               access_buffer[j].address = (uint64_t)addr_normal(engine);
            }
            access_buffer[j].address <<= 6;
      }

      auto start = chrono::high_resolution_clock::now();
      for (int j=0; j<2000; ++j) {
         sys->memAccess(access_buffer[j].address, access_buffer[j].accessType, access_buffer[j].tid);
      }
      auto end = chrono::high_resolution_clock::now();
      run_time += chrono::duration_cast<chrono::duration<double>>(end - start);
   }

   uint64_t accesses = 2000LLU*iterations;
   cout << "Execution time: " << run_time.count() << endl;
   cout << "Accesses: " << accesses << endl;
   cout << "Hits: " << sys->stats.hits << endl;
   cout << "Misses: " << accesses - sys->stats.hits << endl;
   cout << "Local reads: " << sys->stats.local_reads << endl;
   cout << "Local writes: " << sys->stats.local_writes << endl;
   cout << "Remote reads: " << sys->stats.remote_reads << endl;
   cout << "Remote writes: " << sys->stats.remote_writes << endl;
   cout << "Other-cache reads: " << sys->stats.othercache_reads << endl;
   cout << "Compulsory Misses: " << sys->stats.compulsory << endl;
   cout << "Prefetched: " << sys->stats.prefetched << endl;
   
   return 0;
}
