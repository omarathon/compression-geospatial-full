perf stat -r 3 -B -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations $1
