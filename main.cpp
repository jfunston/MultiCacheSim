// Author: Justin Funston
// Date: November 2011
// Email: jfunston@sfu.ca

#include "cache.h"
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
   // The constructor parameters are: Number of caches/domains,
   // the tid_map, the cache line size in bytes,
   // number of cache lines, the associativity,
   // and whether to count compulsory misses
   // WARNING: counting compulsory misses doubles execution time
   System sys(1, tid_map, 64, 1024, 2, false);
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
      if(lines > 10000000) {
         break;
      }
   }

   cout << "Accesses: " << lines << endl;
   cout << "Hits: " << sys.hits << endl;
   cout << "Misses: " << lines - sys.hits << endl;
   cout << "Local reads: " << sys.local_reads << endl;
   cout << "Local writes: " << sys.local_writes << endl;
   cout << "Compulsory Misses: " << sys.compulsory << endl;
   //cout << "Remote reads: " << sys.remote_reads << endl;
   //cout << "Remote writes: " << sys.remote_writes << endl;
   //cout << "Other-cache reads: " << sys.othercache_reads << endl;
   
   infile.close();

   return 0;
}
