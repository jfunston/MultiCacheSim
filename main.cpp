// Author: Justin Funston
// Date: November 2011
// Modified: January 2013
// Email: jfunston@sfu.ca

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
   unsigned int arr_map[] = {0, 1};
   vector<unsigned int> tid_map(arr_map, arr_map + 
         sizeof(arr_map) / sizeof(unsigned int));
   SeqPrefetch prefetch;
   // The constructor parameters are:
   // the tid_map, the cache line size in bytes,
   // number of cache lines, the associativity,
   // whether to count compulsory misses,
   // whether to do virtual to physical translation,
   // and number of caches/domains
   // WARNING: counting compulsory misses doubles execution time
   MultiCacheSystem sys(tid_map, 64, 1024, 64, &prefetch, false, false, 2);
   char rw;
   unsigned long long address;
   unsigned long long lines = 0;
   ifstream infile;
   infile.open("/run/media/jfunston/Seagate Expansion Drive/pinatrace.out", ifstream::in | ifstream::binary);
   assert(infile.is_open());

   while(!infile.eof())
   {
      infile.read(&rw, sizeof(char));
      assert(rw == 'R' || rw == 'W');

      infile.read((char*)&address, sizeof(unsigned long long));
      if(address != 0)
         sys.memAccess(address, rw, 0, false);

      ++lines;
      if(lines >= 500000000) {
         break;
      }
   }

   cout << "Accesses: " << lines << endl;
   cout << "Hits: " << sys.stats.hits << endl;
   cout << "Misses: " << lines - sys.stats.hits << endl;
   cout << "Local reads: " << sys.stats.local_reads << endl;
   cout << "Local writes: " << sys.stats.local_writes << endl;
   //cout << "Compulsory Misses: " << sys.stats.compulsory << endl;
   //cout << "Remote reads: " << sys.remote_reads << endl;
   //cout << "Remote writes: " << sys.remote_writes << endl;
   //cout << "Other-cache reads: " << sys.othercache_reads << endl;
   
   infile.close();

   return 0;
}
