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
   // the tid as an index gives the NUMA domain, so
   // in this case each thread runs in a separate domain
   // except for threads 0 and 1.
   unsigned int arr_map[] = {0,0,1,2,3};
   vector<unsigned int> tid_map(arr_map, arr_map + 
         sizeof(arr_map) / sizeof(unsigned int));
   // The constructor parameters are: Number of caches/domains,
   // the tid_map, the cache line size in bytes,
   // number of cache lines, and the associativity.
   System sys(4, tid_map, 64, 32768, 32);
   unsigned int tid;
   char rw;
   unsigned long long address;
   long lines = 0;
   ifstream infile;
   infile.open("cachedata.out", ifstream::in);
   string line;
   istringstream ss;

   while(getline(infile, line))
   {
      ss.clear();
      ss.str(line);
      ss >> tid;
      ss >> rw;
      ss >> hex >> address;

      assert(rw == 'R' || rw == 'W');
      if(address != 0)
         sys.memAccess(address, rw, tid, false);

      lines++;
   }

   cout << "Accesses: " << lines << endl;
   cout << "Hits: " << sys.hits << endl;
   cout << "Local reads: " << sys.local_reads << endl;
   cout << "Local writes: " << sys.local_writes << endl;
   cout << "Remote reads: " << sys.remote_reads << endl;
   cout << "Remote writes: " << sys.remote_writes << endl;
   cout << "Other-cache reads: " << sys.othercache_reads << endl;
   
   infile.close();

   return 0;
}
