#!/bin/bash

for lines in "1024" "4096"; do
   for assoc in "4" "8" "16"; do
      for prefetch in "sequential" "adjacent" "none"; do
         for comp in "n"; do
            for caches in "1" "4"; do
               for dist in "normal 120000" "uniform 3200000"; do
                  file_name=random_${lines}_${assoc}_${prefetch}_${comp}_${caches}_4_90000_${dist// /}.txt
                  echo $file_name
                  git show --summary >> $file_name
                  cat ../Makefile >> $file_name
                  for i in 1 2 3; do
                     ./random $lines $assoc $prefetch $comp $caches 4 90000 $dist >> $file_name
                  done
               done
            done
         done
      done
   done
done
