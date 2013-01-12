// Author: Justin Funston
// Date: January 2013
// Email: jfunston@sfu.ca

#ifndef THREADED_H
#define THREADED_H

#include <vector>
#include <pthread.h>
#include "system.h"

using namespace std;

struct workItem {
   unsigned long long address;
   char rw;
   unsigned int tid;
   bool is_prefetch;
};

class ThreadedSim {
   vector<vector<workItem> > workPools;
   vector<pthread_t> workers;
   // Each thread will operate on its own system object
   // at the end results will summed
   vector<AdjPrefetchSystem*> workerSystems;
   unsigned int numWorkers;
   unsigned long long SET_MASK;
   unsigned int SET_SHIFT;
   unsigned int numSets;
   friend void* thread_helper(void*);
public:
   static ThreadedSim* sim;
   static const unsigned int batchSize = 10000000;
   ThreadedSim(unsigned int num_workers, unsigned int num_domains, 
                  vector<unsigned int> tid_to_domain, unsigned int line_size, 
                  unsigned int num_lines, unsigned int assoc);
   ~ThreadedSim();
   void processPool();
   SystemStats sumStats();
   vector<workItem> inputPool;
};

#endif
