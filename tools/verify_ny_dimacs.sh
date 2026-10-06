#!/usr/bin/env bash
set -euo pipefail
dir="${1:-benchmarks/data}"
manifest="${2:-benchmarks/data/NY.sha256}"
required=(USA-road-d.NY.gr.gz USA-road-t.NY.gr.gz USA-road-d.NY.co.gz)
for f in "${required[@]}"; do
  test -f "$dir/$f" || { echo "Missing $dir/$f" >&2; echo "Place the NY DIMACS files in $dir or run: tools/fetch_ny_dimacs.sh $dir" >&2; exit 2; }
done
(cd "$dir" && sha256sum -c "$(realpath --relative-to="$dir" "$manifest")")
echo "NY DIMACS corpus verified."
