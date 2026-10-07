#!/usr/bin/env bash
set -euo pipefail
graph="${1:?graph.gr.gz required}"; ops="${2:-200}"; seed="${3:-7}"; reps="${4:-3}"; workload="${5:-mixed}"; coords="${6:-}"; policy="${7:-work}"
if [[ "$workload" != "mixed" && -z "$coords" ]]; then echo "spatial workload requires coordinate file" >&2; exit 2; fi
case "$policy" in work|vertex|fixed) ;; *) echo "policy must be work, vertex, or fixed" >&2; exit 2;; esac
mkdir -p results; stamp="$(date -u +%Y%m%dT%H%M%SZ)"; out="results/bench-${stamp}.csv"; meta="results/bench-${stamp}.meta"
commit="$(git rev-parse HEAD 2>/dev/null || echo unknown)"
cpu="$(awk -F: '/model name/{gsub(/^ /,"",$2);print $2;exit}' /proc/cpuinfo 2>/dev/null || echo unknown)"
ram_kb="$(awk '/MemTotal/{print $2;exit}' /proc/meminfo 2>/dev/null || echo unknown)"
{
 echo "utc=$stamp"; echo "commit=$commit"; echo "uname=$(uname -a)"; echo "cpu=$cpu"; echo "ram_kb=$ram_kb"; echo "threads=1"
 echo "compiler=$(c++ --version | head -n1)"; echo "cmake=$(cmake --version | head -n1)"; echo "build_type=Release"
 echo "graph=$graph"; echo "workload=$workload"; echo "coords=${coords:-none}"; echo "ops=$ops"; echo "seed=$seed"; echo "reps=$reps"; echo "b4_resident_cap=8"; echo "repair_policy=$policy"
} | tee "$meta"
echo "baseline,rep,seed,workload,policy,trace_hash,ops,queries,ns,cold_queries,resident_queries,promotions,rebuilds,filtered_updates,decrease_repairs,increase_repairs,repair_aborts,max_rss_kb" | tee "$out"
oracle_file="$(mktemp)"
oracle_args=(./build/ades_bench "$graph" ORACLE "$ops" "$seed" 0 "$workload")
if [[ -n "$coords" ]]; then oracle_args+=("$coords" "$policy" "$oracle_file"); else oracle_args+=("$policy" "$oracle_file"); fi
"${oracle_args[@]}" >/dev/null
trap 'rm -f "$oracle_file"' EXIT
for ((rep=0;rep<reps;rep++)); do
 baselines=(B0 B1 B2 B3 B4); offset=$((rep % 5))
 for ((k=0;k<5;k++)); do baseline="${baselines[$(((k+offset)%5))]}"
  tmp="$(mktemp)"; rss="$(mktemp)"; args=(./build/ades_bench "$graph" "$baseline" "$ops" "$seed" "$rep" "$workload")
  if [[ -n "$coords" ]]; then args+=("$coords" "$policy" "$oracle_file"); else args+=("$policy" "$oracle_file"); fi
  /usr/bin/time -f '%M' -o "$rss" "${args[@]}" >"$tmp"
  echo "$(cat "$tmp"),$(cat "$rss")" | tee -a "$out"; rm -f "$tmp" "$rss"
 done
done
echo "$out"
