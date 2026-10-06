#!/usr/bin/env bash
set -euo pipefail
graph="${1:?graph.gr.gz required}"
ops="${2:-200}"
seed="${3:-7}"
reps="${4:-3}"
mkdir -p results
stamp="$(date -u +%Y%m%dT%H%M%SZ)"
out="results/bench-${stamp}.csv"
{
  echo "# utc=$stamp"
  echo "# uname=$(uname -a)"
  echo "# compiler=$(c++ --version | head -n1)"
  echo "# cmake=$(cmake --version | head -n1)"
  echo "# graph=$graph"
  ./build/ades_bench "$graph" "$ops" "$seed" "$reps"
} | tee "$out"
echo "$out"
