#!/usr/bin/env bash
set -euo pipefail
bin="${1:-./build/ades_cli}"
dir="${2:-benchmarks/data}"
check(){
 local file="$1" expected="$2"
 local got
 got="$("$bin" "$dir/$file" 1 1000 | tail -n1)"
 test "$got" = "$expected" || { echo "$file: expected $expected, got $got" >&2; exit 1; }
 echo "$file: $got"
}
check USA-road-d.NY.gr.gz 28939
check USA-road-t.NY.gr.gz 61253
