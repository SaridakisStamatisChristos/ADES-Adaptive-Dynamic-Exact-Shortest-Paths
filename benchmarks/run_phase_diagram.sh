#!/usr/bin/env bash
set -euo pipefail
if [[ $# -lt 2 ]]; then echo "usage: $0 GRAPH OUT.csv [queries=10000]"; exit 2; fi
g="$1"; out="$2"; q="${3:-10000}"
echo "baseline,family,seed,trace_sha256,queries,updates,increase_count,decrease_count,update_every,hot_sources,epoch,cap,ns,cold_queries,resident_queries,promotions,evictions,rebuilds,repair_aborts" > "$out"
families=(uniform zipf single-hot rotating-hot hot-pool churn)
updates=(0 1000 200 50 10)
hots=(1 2 4 8 16 32)
for seed in 7 17 29 43 71; do
 for family in "${families[@]}"; do
  for ue in "${updates[@]}"; do
   for hs in "${hots[@]}"; do
    for b in B1 B2 B3 B4; do
     ./build/ades_phase "$g" "$b" "$family" "$q" "$ue" "$hs" 250 "$seed" 8 >> "$out"
    done
   done
  done
 done
done
python3 benchmarks/summarize_phase.py "$out"
