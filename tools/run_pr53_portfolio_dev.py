#!/usr/bin/env python3
from __future__ import annotations
import csv, json, math, statistics, subprocess, sys
from pathlib import Path

PROFILES=("COLD","B3L","ADES-V3","ADES-V5","ADES-V5P")
COMPARATORS=("COLD","B3L","ADES-V3","ADES-V5")
REPETITIONS=2
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

def rotated_profiles(offset):
 n=len(PROFILES); offset%=n
 return PROFILES[offset:]+PROFILES[:offset]

def geomean(values):
 return math.exp(statistics.mean(math.log(float(v)) for v in values))

def main():
 repo=Path('.').resolve(); build=repo/'build'; out=repo/'results/pr53-portfolio-dev'; out.mkdir(parents=True,exist_ok=True); graphs=out/'graphs';graphs.mkdir(exist_ok=True);traces=out/'traces';traces.mkdir(exist_ok=True);oracles=out/'oracles';oracles.mkdir(exist_ok=True)
 gen=build/'ades_workload_v2'; base=build/'ades_pr53_profile'; port=build/'ades_pr53_portfolio_profile'; seeds=(7,17,29); rows=[]
 for ci,(cid,gid,gfile,fam,loc,interval,mode,mag,budgets) in enumerate(cases()):
  gp=repo/gfile if gid=='ny-road-distance' else graphs/gfile
  if not gp.exists(): run([sys.executable,str(repo/'tools/generate_synthetic_graphs.py'),str(graphs),'--only',gid])
  for si,seed in enumerate(seeds):
   stem=f'{ci:02d}-{cid}-s{seed}'; tr=traces/f'{stem}.trace'; meta=traces/f'{stem}.json'; oracle=oracles/f'{stem}.oracle'
   run([str(gen),str(gp),str(tr),str(meta),str(seed),'160',fam,str(interval),mode,mag,'4','15',str(loc),'4'])
   run([str(base),'oracle',str(gp),str(tr),str(oracle)])
   for bi,budget in enumerate(budgets):
    for rep in range(REPETITIONS):
     # Rotate isolated-process execution order so no candidate is systematically
     # measured after the expensive baselines. Across 3 seeds x 2 repetitions,
     # every profile occupies every order position at least once up to one
     # unavoidable duplicate position.
     order=rotated_profiles(ci+bi+si*REPETITIONS+rep)
     for position,profile in enumerate(order):
      if profile=='ADES-V5P':
       r=parse(run([str(port),str(gp),str(tr),str(oracle),str(budget),profile]))
      else:
       r=parse(run([str(base),'run',str(gp),str(tr),str(oracle),str(budget),profile]))
      rows.append(dict(case=cid,budget=budget,seed=seed,repetition=rep,order_position=position,**r))
 with (out/'measurements.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)

 # Collapse repeated measurements within each independent seed in log space,
 # matching the frozen PR49/PR50 statistical convention. Confidence intervals
 # are then computed across the three independent seeds, not six pseudo-samples.
 by_seed={}
 for r in rows:
  key=(r['case'],int(r['budget']),int(r['seed']),r['profile'])
  by_seed.setdefault(key,[]).append(int(r['algorithm_ns']))
 for key,values in by_seed.items():
  if len(values)!=REPETITIONS: raise RuntimeError(f'incomplete repetitions for {key}: {len(values)}')
 by_seed={key:geomean(values) for key,values in by_seed.items()}

 regimes={}
 for (case,budget,seed,profile),value in by_seed.items():
  regimes.setdefault((case,budget),{}).setdefault(profile,{})[seed]=value
 summary=[]; regressions=[]; wins=[]; fastest={}
 for (case,budget),p in sorted(regimes.items()):
  missing=[profile for profile in PROFILES if profile not in p]
  if missing: raise RuntimeError(f'missing profiles for {(case,budget)}: {missing}')
  med={profile:statistics.median(seed_values.values()) for profile,seed_values in p.items()}
  winner=min(med,key=med.get);fastest[winner]=fastest.get(winner,0)+1;best=med[winner]
  summary.append(dict(case=case,budget=budget,winner=winner,best_ns=int(round(best)),v5p_ns=int(round(med['ADES-V5P'])),v5p_over_best=med['ADES-V5P']/best,**{f'ns_{k}':int(round(v)) for k,v in med.items()}))
  for comp in COMPARATORS:
   common=sorted(set(p[comp])&set(p['ADES-V5P']))
   if len(common)!=len(seeds): raise RuntimeError(f'incomplete seed pairing for {(case,budget,comp)}')
   ratios=[p[comp][seed]/p['ADES-V5P'][seed] for seed in common]
   logs=[math.log(x) for x in ratios]; mean=statistics.mean(logs); sd=statistics.stdev(logs);half=T95*sd/math.sqrt(len(logs));gm=math.exp(mean);lo=math.exp(mean-half);hi=math.exp(mean+half)
   rec=dict(case=case,budget=budget,comparator=comp,geomean_comp_over_v5p=gm,ci95_low=lo,ci95_high=hi)
   if gm<=1/1.10 and hi<1: regressions.append(rec)
   if gm>=1.10 and lo>1: wins.append(rec)
 with (out/'summary.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(summary[0]));w.writeheader();w.writerows(summary)
 decision=dict(
  regimes=len(summary),
  repetitions_per_seed=REPETITIONS,
  independent_seeds=len(seeds),
  execution_order='rotated isolated processes',
  within_seed_aggregation='geometric mean',
  material_regressions=len(regressions),
  material_wins=len(wins),
  fastest_profile_counts=fastest,
  regressions=regressions,
 )
 (out/'decision.json').write_text(json.dumps(decision,indent=2)+'\n');print(json.dumps(decision,indent=2))
if __name__=='__main__':main()