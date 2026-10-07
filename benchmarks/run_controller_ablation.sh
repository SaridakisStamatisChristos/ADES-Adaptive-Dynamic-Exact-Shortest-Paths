#!/usr/bin/env bash
set -euo pipefail
graph="${1:?graph required}"; rounds="${2:-20}"; seed="${3:-7}"; reps="${4:-5}"
mkdir -p results; stamp="$(date -u +%Y%m%dT%H%M%SZ)"; out="results/controller-${stamp}.csv"
echo "regime,policy,seed,rounds,source,ns,rebuilds,increase_repairs,repair_aborts,filtered_updates,max_rss_kb,rep" | tee "$out"
for ((rep=0;rep<reps;rep++)); do
 for regime in small catastrophic; do
  for policy in fixed vertex work; do
   tmp="$(mktemp)"; rss="$(mktemp)"
   /usr/bin/time -f '%M' -o "$rss" ./build/ades_controller_workload "$graph" "$regime" "$rounds" "$seed" "$policy" >"$tmp"
   echo "$(cat "$tmp"),$(cat "$rss"),$rep" | tee -a "$out";rm -f "$tmp" "$rss"
  done
 done
done
echo "$out"
