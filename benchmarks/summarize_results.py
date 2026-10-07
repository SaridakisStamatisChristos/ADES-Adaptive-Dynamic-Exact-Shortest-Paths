#!/usr/bin/env python3
import csv,glob,statistics,sys
from collections import defaultdict
files=glob.glob("results/*.csv")
matrix=defaultdict(list); controller=defaultdict(list)
for path in files:
 metric=path.split("/")[-1].split("-")[1] if path.split("/")[-1].startswith("USA-road-") else "unknown"
 with open(path,newline="") as f:
  r=csv.DictReader(f)
  if not r.fieldnames: continue
  if "baseline" in r.fieldnames:
   rows=list(r)
   bytrace=defaultdict(set)
   for x in rows: bytrace[(x["seed"],x["workload"],x["rep"])].add(x["trace_hash"])
   bad=[k for k,v in bytrace.items() if len(v)!=1]
   if bad: raise SystemExit(f"trace fingerprint mismatch in {path}: {bad[:3]}")
   for x in rows: matrix[(metric,x["workload"],x["baseline"])].append((int(x["ns"]),int(x["max_rss_kb"])))
  elif "regime" in r.fieldnames:
   for x in r: controller[(metric,x["regime"],x["policy"])].append((int(x["ns"]),int(x["max_rss_kb"]),int(x["rebuilds"]),int(x["increase_repairs"]),int(x["repair_aborts"])))
def pct(a,p):
 a=sorted(a);return a[min(len(a)-1,max(0,int(round((len(a)-1)*p))))]
with open("results/SUMMARY.md","w") as o:
 o.write("# ADES benchmark summary\n\nGenerated from raw evidence CSVs. Times are per isolated process measurement; median/p95 are descriptive, not confidence intervals.\n\n")
 o.write("## B0-B4 matrix\n\n| metric | workload | baseline | n | median ms | p95 ms | median RSS MiB |\n|---|---|---:|---:|---:|---:|---:|\n")
 for (m,w,b),v in sorted(matrix.items()):
  ns=[x[0] for x in v];rss=[x[1] for x in v]
  o.write(f"| {m} | {w} | {b} | {len(v)} | {statistics.median(ns)/1e6:.3f} | {pct(ns,.95)/1e6:.3f} | {statistics.median(rss)/1024:.2f} |\n")
 o.write("\n## Controller ablation\n\n| metric | regime | policy | n | median ms | p95 ms | median RSS MiB | rebuilds | repairs | aborts |\n|---|---|---|---:|---:|---:|---:|---:|---:|---:|\n")
 for (m,g,p),v in sorted(controller.items()):
  ns=[x[0] for x in v];rss=[x[1] for x in v]
  o.write(f"| {m} | {g} | {p} | {len(v)} | {statistics.median(ns)/1e6:.3f} | {pct(ns,.95)/1e6:.3f} | {statistics.median(rss)/1024:.2f} | {sum(x[2] for x in v)} | {sum(x[3] for x in v)} | {sum(x[4] for x in v)} |\n")
print("results/SUMMARY.md")
