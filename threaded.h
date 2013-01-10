// Author: Justin Funston
// Date: January 2013
// Email: jfunston@sfu.ca

#ifndef THREADED_H
#define THREADED_H

struct workItem {
   unsigned long long address;
   char rw;
   bool is_prefetch;
};

class Threaded {
   static const unsigned int batchSize = 10000;
};

#endif
