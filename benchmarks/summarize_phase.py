#!/usr/bin/env python3
import csv,statistics,sys
from collections import defaultdict
p=sys.argv[1]; rows=list(csv.DictReader(open(p,newline="")))
cells=defaultdict(lambda:defaultdict(list))
for r in rows:
 k=(r["family"],int(r["update_every"]),int(r["hot_sources"]))
 timing_key="algorithm_ns" if "algorithm_ns" in r and r["algorithm_ns"] not in (None,"") else "ns"
 cells[k][r["baseline"]].append(int(r[timing_key]))
out=p.rsplit(".",1)[0]+"-summary.csv"
with open(out,"w",newline="") as f:
 w=csv.writer(f);w.writerow(["family","update_every","hot_sources","b1_median_ns","b4_median_ns","speedup_b1_over_b4","classification","seeds"])
 for k,d in sorted(cells.items()):
  if "B1" not in d or "B4" not in d: continue
  b1=statistics.median(d["B1"]);b4=statistics.median(d["B4"]);ratio=b1/b4
  cls="ADES-superior" if ratio>=1.10 else ("B1-superior" if ratio<=0.90 else "parity")
  w.writerow([*k,int(b1),int(b4),f"{ratio:.6f}",cls,min(len(d["B1"]),len(d["B4"]))])
print(out)
