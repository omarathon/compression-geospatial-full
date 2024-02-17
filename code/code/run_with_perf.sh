# perf stat -e L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,L1-dcache-store-misses,L1-icache-loads,L1-icache-load-misses,LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses,dTLB-loads,dTLB-load-misses,iTLB-loads,iTLB-load-misses,cache-references,cache-misses,bus-cycles $1

perf stat -r 3 -B -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations $1
