#!/usr/bin/env bash
set -euo pipefail
graph="${1:?graph.gr.gz required}"; ops="${2:-200}"; seed="${3:-7}"; reps="${4:-3}"
mkdir -p results; stamp="$(date -u +%Y%m%dT%H%M%SZ)"; out="results/bench-${stamp}.csv"; meta="results/bench-${stamp}.meta"
commit="$(git rev-parse HEAD 2>/dev/null || echo unknown)"
cpu="$(awk -F: '/model name/{gsub(/^ /,"",$2);print $2;exit}' /proc/cpuinfo 2>/dev/null || echo unknown)"
ram_kb="$(awk '/MemTotal/{print $2;exit}' /proc/meminfo 2>/dev/null || echo unknown)"
{
 echo "utc=$stamp"; echo "commit=$commit"; echo "uname=$(uname -a)"
 echo "cpu=$cpu"; echo "ram_kb=$ram_kb"; echo "threads=1"
 echo "compiler=$(c++ --version | head -n1)"; echo "cmake=$(cmake --version | head -n1)"
 echo "build_type=Release"; echo "graph=$graph"; echo "ops=$ops"; echo "seed=$seed"; echo "reps=$reps"
 echo "b4_resident_cap=8"
} | tee "$meta"
echo "baseline,rep,seed,ops,queries,ns,max_rss_kb" | tee "$out"
for ((rep=0;rep<reps;rep++)); do
 for baseline in B0 B1 B2 B3 B4; do
  tmp="$(mktemp)"; rss="$(mktemp)"
  /usr/bin/time -f '%M' -o "$rss" ./build/ades_bench "$graph" "$baseline" "$ops" "$seed" "$rep" >"$tmp"
  line="$(cat "$tmp")"; echo "$line,$(cat "$rss")" | tee -a "$out"
  rm -f "$tmp" "$rss"
 done
done
echo "$out"
