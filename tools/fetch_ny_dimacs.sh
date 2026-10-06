#!/usr/bin/env bash
set -euo pipefail
out="${1:-benchmarks/data}"
manifest="${2:-benchmarks/data/NY.sha256}"
mkdir -p "$out"
out="$(realpath "$out")"
manifest="$(realpath "$manifest")"
base="https://www.diag.uniroma1.it/challenge9/data"
curl --fail --location --retry 3 "$base/USA-road-d/USA-road-d.NY.gr.gz" -o "$out/USA-road-d.NY.gr.gz"
curl --fail --location --retry 3 "$base/USA-road-t/USA-road-t.NY.gr.gz" -o "$out/USA-road-t.NY.gr.gz"
curl --fail --location --retry 3 "$base/USA-road-d/USA-road-d.NY.co.gz" -o "$out/USA-road-d.NY.co.gz"
(cd "$out" && sha256sum -c "$manifest")
