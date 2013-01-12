// Author: Justin Funston
// Date: November 2011
// Modified: January 2013
// Email: jfunston@sfu.ca

#include "threaded_sim.h"
#include "system.h"
#include <iostream>
#include <fstream>
#include <cassert>
#include <sstream>
#include <string>

using namespace std;

int main()
{
   // tid_map is used to inform the simulator how
   // thread ids map to NUMA/cache domains. Using
   // the tid as an index gives the NUMA domain.
   unsigned int arr_map[] = {0};
   vector<unsigned int> tid_map(arr_map, arr_map + 
         sizeof(arr_map) / sizeof(unsigned int));
   // The constructor parameters are: 
   // Number of worker threads
   // Number of caches/domains,
   // the tid_map, the cache line size in bytes,
   // number of cache lines, the associativity,
   // whether to count compulsory misses,
   // and whether to do virtual to physical translation
   // WARNING: counting compulsory misses doubles execution time
   ThreadedSim sim(4, 1, tid_map, 64, 1024, 64);
   unsigned long long lines = 0;
   SystemStats sys;
   ifstream infile;
   infile.open("/run/media/jfunston/Seagate Expansion Drive/pinatrace.out", ifstream::in | ifstream::binary);
   assert(infile.is_open());

   while(!infile.eof())
   {
      workItem temp;

      sim.inputPool.clear();
      temp.tid = 0;
      temp.is_prefetch = false;

      for(unsigned int i=0; i<sim.batchSize; ++i) {
         infile.read(&(temp.rw), sizeof(char));
         assert(temp.rw == 'R' || temp.rw == 'W');
         infile.read((char*)&(temp.address), sizeof(unsigned long long));

         if(temp.address != 0) {
            sim.inputPool.push_back(temp);
            ++lines;
         }
         if(infile.eof() || lines >= 500000000) {
            break;
         }
      }

      sim.processPool();
      if(lines >= 500000000) {
         break;
      }
   }

   sys = sim.sumStats();
   cout << "Accesses: " << lines << endl;
   cout << "Hits: " << sys.hits << endl;
   cout << "Misses: " << lines - sys.hits << endl;
   cout << "Local reads: " << sys.local_reads << endl;
   cout << "Local writes: " << sys.local_writes << endl;
   //cout << "Compulsory Misses: " << sys..compulsory << endl;
   //cout << "Remote reads: " << sys.remote_reads << endl;
   //cout << "Remote writes: " << sys.remote_writes << endl;
   //cout << "Other-cache reads: " << sys.othercache_reads << endl;
   
   infile.close();

   return 0;
}
