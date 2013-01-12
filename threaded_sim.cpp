// Author: Justin Funston
// Date: January 2013
// Email: jfunston@sfu.ca

#include <cassert>
#include <cmath>
#include "threaded_sim.h"

ThreadedSim* ThreadedSim::sim;

void* thread_helper(void* in_ptr)
{
   unsigned long long id = (unsigned long long) in_ptr;

   for(unsigned int i=0; i < ThreadedSim::sim->workPools[id].size(); ++i) {
      workItem* work = &(ThreadedSim::sim->workPools[id][i]);
      ThreadedSim::sim->workerSystems[id]->memAccess(work->address,
         work->rw, work->tid, work->is_prefetch);
   }

   return NULL;
}

ThreadedSim::ThreadedSim(unsigned int num_workers, unsigned int num_domains, 
                  vector<unsigned int> tid_to_domain, unsigned int line_size, 
                  unsigned int num_lines, unsigned int assoc)
{
   // Each worker will process independent sets,
   // there needs to be at least one set per worker
   assert((num_lines / assoc) > num_workers);
   inputPool.resize(batchSize);
   workPools.resize(num_workers);
   workers.resize(num_workers);
   for(unsigned int i=0; i<num_workers; ++i) {
      AdjPrefetchSystem* temp = new AdjPrefetchSystem(num_domains, 
         tid_to_domain, line_size, num_lines, assoc);
      workerSystems.push_back(temp);
   }
   SET_SHIFT = log2(line_size);
   SET_MASK = ((num_lines / assoc) - 1) << SET_SHIFT;
   numSets = num_lines / assoc;
   numWorkers = num_workers;
   sim = this;
}

ThreadedSim::~ThreadedSim()
{
   for(unsigned int i=0; i<numWorkers; ++i) {
      delete workerSystems[i];
   }
}

SystemStats ThreadedSim::sumStats()
{
   SystemStats stats;

   for(unsigned int i=0; i<numWorkers; ++i) {
      stats.hits += workerSystems[i]->stats.hits;
      stats.local_reads += workerSystems[i]->stats.local_reads;
      stats.remote_reads += workerSystems[i]->stats.remote_reads;
      stats.othercache_reads += workerSystems[i]->stats.othercache_reads;
      stats.local_writes += workerSystems[i]->stats.local_writes;
      stats.remote_writes += workerSystems[i]->stats.remote_writes;
   }

   return stats;
}

void ThreadedSim::processPool()
{
   int sets_per_worker = numSets / numWorkers;

   for(unsigned int i=0; i<numWorkers; ++i) {
      workPools[i].clear();
   }

   for(unsigned int i=0; i<inputPool.size(); ++i) {
      unsigned int set, prefetch_set;
      workItem temp;
      unsigned int worker;

      set = (inputPool[i].address & SET_MASK) >> SET_SHIFT;
      worker = (set / sets_per_worker) % numWorkers;
      workPools[worker].push_back(inputPool[i]);

      prefetch_set = (set + 1) % numSets;
      if(worker != (prefetch_set / sets_per_worker) % numWorkers) {
         temp.address = inputPool[i].address;
         temp.address = temp.address + (1 << SET_SHIFT);
         temp.rw = 'R';
         temp.is_prefetch = true;
         worker = (prefetch_set / sets_per_worker) % numWorkers;
         workPools[worker].push_back(temp);
      }
   }

   for(unsigned int i=0; i<numWorkers; ++i) {
      pthread_create(&(workers[i]), NULL, thread_helper, (void*)i);
   }
   for(unsigned int i=0; i<numWorkers; ++i) {
     pthread_join(workers[i], NULL);
   }
}

