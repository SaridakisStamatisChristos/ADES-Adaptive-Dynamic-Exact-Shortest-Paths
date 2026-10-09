#!/usr/bin/env python3
from __future__ import annotations
import csv, hashlib, json, math, statistics, subprocess, sys
from pathlib import Path

PROFILES=("COLD","B3L","FREQ-LRU-REPAIR","ADES-V2","ADES-V3","ADES-V4","ADES-V5")
FIELDS=("profile","trace_sha256","queries","updates","budget_bytes","algorithm_ns","query_ns","update_ns","query_p50_ns","query_p95_ns","accounted_bytes","peak_accounted_bytes","cold_queries","resident_queries","promotions","evictions","rebuilds","memory_budget_rejections","progressive_queries","partial_creations","partial_invalidations","progressive_promotions","direct_promotions","hazard_rejections","direct_fit_rejections")
T95=4.302652729911275

def run(cmd,timeout=1800):
    p=subprocess.run(cmd,text=True,capture_output=True,timeout=timeout)
    if p.returncode: raise RuntimeError(f"failed: {' '.join(map(str,cmd))}\n{p.stderr[-4000:]}")
    return p.stdout.strip()

def sha(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
      for b in iter(lambda:f.read(1<<20),b''): h.update(b)
    return h.hexdigest()

def parse(s):
    p=s.split(',')
    if len(p)!=len(FIELDS): raise RuntimeError(f"field mismatch {len(p)} != {len(FIELDS)}")
    return dict(zip(FIELDS,p))

def cases():
    M=1024*1024
    return [
      dict(id='ny-churn-u5',graph='ny-road-distance',file='benchmarks/data/USA-road-d.NY.gr.gz',family='churn',loc=100,interval=5,mode='strict-alternating',mag='medium',budgets=(16*M,64*M)),
      dict(id='grid-churn-u5',graph='syn-grid-224x224-v1',file='syn-grid-224x224-v1.gr',family='churn',loc=100,interval=5,mode='strict-alternating',mag='medium',budgets=(16*M,64*M)),
      dict(id='grid-zipf-u50',graph='syn-grid-224x224-v1',file='syn-grid-224x224-v1.gr',family='zipf',loc=75,interval=50,mode='decrease-only',mag='medium',budgets=(20*M,40*M)),
      dict(id='uniform-churn-u5',graph='syn-uniform-50000-d6-v1',file='syn-uniform-50000-d6-v1.gr',family='churn',loc=90,interval=5,mode='decrease-only',mag='medium',budgets=(20*M,40*M)),
      dict(id='clustered-churn-u5',graph='syn-clustered-50000-c100-d6-v1',file='syn-clustered-50000-c100-d6-v1.gr',family='churn',loc=90,interval=5,mode='strict-alternating',mag='small',budgets=(20*M,40*M)),
      dict(id='ny-hotpool-u10',graph='ny-road-distance',file='benchmarks/data/USA-road-d.NY.gr.gz',family='hot-pool',loc=70,interval=10,mode='balanced-random',mag='small',budgets=(24*M,)),
      dict(id='scale-hotpool-u10',graph='syn-scale-free-50000-m4-v1',file='syn-scale-free-50000-m4-v1.gr',family='hot-pool',loc=70,interval=10,mode='balanced-random',mag='small',budgets=(24*M,)),
    ]

def ensure_graph(repo,out,c):
    if c['graph']=='ny-road-distance': return repo/c['file']
    p=out/c['file']
    if not p.exists(): run([sys.executable,str(repo/'tools/generate_synthetic_graphs.py'),str(out),'--only',c['graph']])
    return p

def main():
    repo=Path('.').resolve(); build=repo/'build'; out=repo/'results/pr53-development'; out.mkdir(parents=True,exist_ok=True)
    graphs=out/'graphs'; traces=out/'traces'; oracles=out/'oracles'; graphs.mkdir(exist_ok=True); traces.mkdir(exist_ok=True); oracles.mkdir(exist_ok=True)
    gen=build/'ades_workload_v2'; runner=build/'ades_pr53_profile'
    seeds=(7,17,29); rows=[]
    for ci,c in enumerate(cases()):
      gp=ensure_graph(repo,graphs,c); gsha=sha(gp)
      for seed in seeds:
        stem=f"{ci:02d}-{c['id']}-s{seed}"; tr=traces/f'{stem}.trace'; meta=traces/f'{stem}.json'; oracle=oracles/f'{stem}.oracle'
        run([str(gen),str(gp),str(tr),str(meta),str(seed),'160',c['family'],str(c['interval']),c['mode'],c['mag'],'4','15',str(c['loc']),'4'])
        run([str(runner),'oracle',str(gp),str(tr),str(oracle)])
        tsha=json.loads(meta.read_text())['trace_sha256']
        for budget in c['budgets']:
          for profile in PROFILES:
            r=parse(run([str(runner),'run',str(gp),str(tr),str(oracle),str(budget),profile]))
            if r['trace_sha256']!=tsha or int(r['peak_accounted_bytes'])>budget: raise RuntimeError('identity/budget failure')
            rows.append(dict(case=c['id'],graph=c['graph'],graph_sha256=gsha,family=c['family'],interval=c['interval'],mode=c['mode'],magnitude=c['mag'],budget=budget,seed=seed,**r))
    with (out/'measurements.csv').open('w',newline='') as f:
      w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)

    regime={}
    for r in rows:
      key=(r['case'],int(r['budget'])); regime.setdefault(key,{}).setdefault(r['profile'],[]).append(int(r['algorithm_ns']))
    summary=[]; fastest={}; near=0
    for key,p in sorted(regime.items()):
      meds={k:int(statistics.median(v)) for k,v in p.items()}; best=min(meds.values()); winner=min(meds,key=meds.get); fastest[winner]=fastest.get(winner,0)+1
      if meds['ADES-V5']<=math.ceil(best*1.01): near+=1
      summary.append(dict(case=key[0],budget=key[1],winner=winner,best_ns=best,v5_ns=meds['ADES-V5'],v5_over_best=f"{meds['ADES-V5']/best:.6f}",**{f"ns_{k}":v for k,v in meds.items()}))
    with (out/'summary.csv').open('w',newline='') as f:
      w=csv.DictWriter(f,fieldnames=list(summary[0]));w.writeheader();w.writerows(summary)

    confidence=[]; material_regressions=0; material_wins=0
    for key,p in sorted(regime.items()):
      v5=p['ADES-V5']
      for comp in PROFILES[:-1]:
        ratios=[a/b for a,b in zip(p[comp],v5)]; logs=[math.log(x) for x in ratios]; mean=statistics.mean(logs); gm=math.exp(mean); sd=statistics.stdev(logs); half=T95*sd/math.sqrt(3); lo=math.exp(mean-half); hi=math.exp(mean+half)
        if gm<=1/1.10 and hi<1: material_regressions+=1
        if gm>=1.10 and lo>1: material_wins+=1
        confidence.append(dict(case=key[0],budget=key[1],comparator=comp,geomean_comp_over_v5=f'{gm:.9f}',ci95_low=f'{lo:.9f}',ci95_high=f'{hi:.9f}'))
    with (out/'confidence.csv').open('w',newline='') as f:
      w=csv.DictWriter(f,fieldnames=list(confidence[0]));w.writeheader();w.writerows(confidence)
    decision=dict(regimes=len(summary),v5_fastest_or_within_1pct=near,material_regressions=material_regressions,material_wins=material_wins,fastest_profile_counts=fastest)
    (out/'decision.json').write_text(json.dumps(decision,indent=2)+'\n')
    print(json.dumps(decision,indent=2))
if __name__=='__main__': main()
