#!/usr/bin/env python3
from __future__ import annotations
import csv, json, math, statistics, subprocess, sys
from pathlib import Path

BASE=("COLD","B3L","ADES-V3","ADES-V5")
T95=4.302652729911275

def run(cmd):
 p=subprocess.run(cmd,text=True,capture_output=True,timeout=1800)
 if p.returncode: raise RuntimeError(f"failed: {' '.join(map(str,cmd))}\n{p.stderr[-4000:]}")
 return p.stdout.strip()

def parse(line):
 p=line.split(',')
 if len(p)!=25: raise RuntimeError(f'expected 25 fields, got {len(p)}: {line}')
 keys=("profile","trace_sha256","queries","updates","budget_bytes","algorithm_ns","query_ns","update_ns","query_p50_ns","query_p95_ns","accounted_bytes","peak_accounted_bytes","cold_queries","resident_queries","promotions","evictions","rebuilds","memory_budget_rejections","progressive_queries","partial_creations","partial_invalidations","progressive_promotions","direct_promotions","hazard_rejections","direct_fit_rejections")
 return dict(zip(keys,p))

def cases():
 M=1024*1024
 return [
  ('ny-churn-u5','ny-road-distance','benchmarks/data/USA-road-d.NY.gr.gz','churn',100,5,'strict-alternating','medium',(16*M,64*M)),
  ('grid-churn-u5','syn-grid-224x224-v1','syn-grid-224x224-v1.gr','churn',100,5,'strict-alternating','medium',(16*M,64*M)),
  ('grid-zipf-u50','syn-grid-224x224-v1','syn-grid-224x224-v1.gr','zipf',75,50,'decrease-only','medium',(20*M,40*M)),
  ('uniform-churn-u5','syn-uniform-50000-d6-v1','syn-uniform-50000-d6-v1.gr','churn',90,5,'decrease-only','medium',(20*M,40*M)),
  ('clustered-churn-u5','syn-clustered-50000-c100-d6-v1','syn-clustered-50000-c100-d6-v1.gr','churn',90,5,'strict-alternating','small',(20*M,40*M)),
  ('ny-hotpool-u10','ny-road-distance','benchmarks/data/USA-road-d.NY.gr.gz','hot-pool',70,10,'balanced-random','small',(24*M,)),
  ('scale-hotpool-u10','syn-scale-free-50000-m4-v1','syn-scale-free-50000-m4-v1.gr','hot-pool',70,10,'balanced-random','small',(24*M,)),
 ]

def main():
 repo=Path('.').resolve(); build=repo/'build'; out=repo/'results/pr53-portfolio-dev'; out.mkdir(parents=True,exist_ok=True); graphs=out/'graphs';graphs.mkdir(exist_ok=True);traces=out/'traces';traces.mkdir(exist_ok=True);oracles=out/'oracles';oracles.mkdir(exist_ok=True)
 gen=build/'ades_workload_v2'; base=build/'ades_pr53_profile'; port=build/'ades_pr53_portfolio_profile'; seeds=(7,17,29); rows=[]
 for ci,(cid,gid,gfile,fam,loc,interval,mode,mag,budgets) in enumerate(cases()):
  gp=repo/gfile if gid=='ny-road-distance' else graphs/gfile
  if not gp.exists(): run([sys.executable,str(repo/'tools/generate_synthetic_graphs.py'),str(graphs),'--only',gid])
  for seed in seeds:
   stem=f'{ci:02d}-{cid}-s{seed}'; tr=traces/f'{stem}.trace'; meta=traces/f'{stem}.json'; oracle=oracles/f'{stem}.oracle'
   run([str(gen),str(gp),str(tr),str(meta),str(seed),'160',fam,str(interval),mode,mag,'4','15',str(loc),'4'])
   run([str(base),'oracle',str(gp),str(tr),str(oracle)])
   for budget in budgets:
    for profile in BASE:
     r=parse(run([str(base),'run',str(gp),str(tr),str(oracle),str(budget),profile]));rows.append(dict(case=cid,budget=budget,seed=seed,**r))
    r=parse(run([str(port),str(gp),str(tr),str(oracle),str(budget),'ADES-V5P']));rows.append(dict(case=cid,budget=budget,seed=seed,**r))
 with (out/'measurements.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 regimes={}
 for r in rows: regimes.setdefault((r['case'],int(r['budget'])),{}).setdefault(r['profile'],[]).append(int(r['algorithm_ns']))
 summary=[]; regressions=[]; wins=[]; fastest={}
 for (case,budget),p in sorted(regimes.items()):
  med={k:int(statistics.median(v)) for k,v in p.items()}; winner=min(med,key=med.get);fastest[winner]=fastest.get(winner,0)+1;best=med[winner]
  summary.append(dict(case=case,budget=budget,winner=winner,best_ns=best,v5p_ns=med['ADES-V5P'],v5p_over_best=med['ADES-V5P']/best,**{f'ns_{k}':v for k,v in med.items()}))
  for comp in BASE:
   ratios=[a/b for a,b in zip(p[comp],p['ADES-V5P'])]; logs=[math.log(x) for x in ratios]; mean=statistics.mean(logs); sd=statistics.stdev(logs);half=T95*sd/math.sqrt(3);gm=math.exp(mean);lo=math.exp(mean-half);hi=math.exp(mean+half)
   rec=dict(case=case,budget=budget,comparator=comp,geomean_comp_over_v5p=gm,ci95_low=lo,ci95_high=hi)
   if gm<=1/1.10 and hi<1: regressions.append(rec)
   if gm>=1.10 and lo>1: wins.append(rec)
 with (out/'summary.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(summary[0]));w.writeheader();w.writerows(summary)
 decision=dict(regimes=len(summary),material_regressions=len(regressions),material_wins=len(wins),fastest_profile_counts=fastest,regressions=regressions)
 (out/'decision.json').write_text(json.dumps(decision,indent=2)+'\n');print(json.dumps(decision,indent=2))
if __name__=='__main__':main()
