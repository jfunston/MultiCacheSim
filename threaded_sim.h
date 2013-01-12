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
   bool is_prefetch;
};

class ThreadedSim {
   static const unsigned int batchSize = 10000;
   vector<vector<workItem> > workPools;
   vector<pthread_t> workers;
   unsigned int num_workers;
public:
   ThreadedSim(unsigned int num_workers, AdjPrefetchSystem system);
};

#endif
