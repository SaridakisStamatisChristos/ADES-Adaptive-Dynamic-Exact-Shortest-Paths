#!/usr/bin/env bash
set -euo pipefail
out="${1:-benchmarks/data}"
manifest="${2:-benchmarks/data/NY.sha256}"
mkdir -p "$out"
out="$(realpath "$out")"
manifest="$(realpath "$manifest")"
base="https://www.diag.uniroma1.it/challenge9/data"

cleanup_parts(){ rm -f "$out"/*.part; }
trap cleanup_parts EXIT

download(){
  local url="$1" dest="$2" tmp="${2}.part"
  rm -f "$tmp"
  curl --fail --location \
    --retry 8 --retry-all-errors --retry-delay 2 \
    --connect-timeout 20 --max-time 600 \
    "$url" -o "$tmp"
  mv "$tmp" "$dest"
}

download "$base/USA-road-d/USA-road-d.NY.gr.gz" "$out/USA-road-d.NY.gr.gz"
download "$base/USA-road-t/USA-road-t.NY.gr.gz" "$out/USA-road-t.NY.gr.gz"
download "$base/USA-road-d/USA-road-d.NY.co.gz" "$out/USA-road-d.NY.co.gz"
(cd "$out" && sha256sum -c "$manifest")