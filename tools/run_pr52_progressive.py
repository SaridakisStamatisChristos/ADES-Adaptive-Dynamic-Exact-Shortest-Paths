#!/usr/bin/env python3
"""PR52 ADES-V4 progressive-frontier regression + third holdout benchmark."""
from __future__ import annotations
import argparse, csv, hashlib, json, math, os, statistics, subprocess, sys, time
from pathlib import Path

PROFILES=("COLD","B3L","FREQ-LRU-REPAIR","ADES","ADES-V2","ADES-V3","ADES-V4")
COMPARATORS=PROFILES[:-1]
PROFILE_FIELDS=("profile","trace_sha256","queries","updates","increase_count","decrease_count","budget_bytes","algorithm_ns","query_ns","update_ns","operation_p50_ns","operation_p95_ns","query_p50_ns","query_p95_ns","update_p50_ns","update_p95_ns","accounted_bytes","peak_accounted_bytes","cold_queries","resident_queries","promotions","evictions","rebuilds","repair_aborts","filtered_updates","decrease_repairs","increase_repairs","memory_budget_rejections","progressive_queries","partial_creations","partial_evictions","partial_invalidations","partial_completions","progressive_promotions")
META_FIELDS=("phase","graph_id","graph_sha256","family","locality_percent","update_interval","update_mode","magnitude","seed","repeat","execution_order")
MEASUREMENT_FIELDS=(*META_FIELDS,*PROFILE_FIELDS,"peak_rss_kb","wall_s")
T95_DF2=4.302652729911275


def sha256_file(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda:f.read(1<<20),b""): h.update(chunk)
    return h.hexdigest()

def run_checked(cmd,timeout=1800):
    p=subprocess.run(cmd,text=True,capture_output=True,timeout=timeout,check=False)
    if p.returncode: raise RuntimeError(f"command failed ({p.returncode}): {' '.join(map(str,cmd))}\n{p.stderr[-4000:]}")
    return p

def parse_profile(text):
    parts=text.strip().split(",")
    if len(parts)!=len(PROFILE_FIELDS): raise ValueError(f"profile fields {len(parts)} != {len(PROFILE_FIELDS)}")
    return dict(zip(PROFILE_FIELDS,parts))

def regression_cases():
    mib=1024*1024
    return [
      dict(graph_id="ny-road-distance",graph_file="benchmarks/data/USA-road-d.NY.gr.gz",family="churn",locality=100,interval=5,mode="strict-alternating",magnitude="medium",budgets=(16*mib,64*mib)),
      dict(graph_id="ny-road-distance",graph_file="benchmarks/data/USA-road-d.NY.gr.gz",family="churn",locality=100,interval=50,mode="strict-alternating",magnitude="medium",budgets=(16*mib,64*mib)),
      dict(graph_id="syn-grid-224x224-v1",graph_file="syn-grid-224x224-v1.gr",family="churn",locality=100,interval=5,mode="strict-alternating",magnitude="medium",budgets=(16*mib,64*mib)),
      dict(graph_id="syn-grid-224x224-v1",graph_file="syn-grid-224x224-v1.gr",family="churn",locality=100,interval=50,mode="strict-alternating",magnitude="medium",budgets=(16*mib,64*mib)),
      dict(graph_id="ny-road-distance",graph_file="benchmarks/data/USA-road-d.NY.gr.gz",family="hot-pool",locality=70,interval=10,mode="balanced-random",magnitude="small",budgets=(24*mib,)),
      dict(graph_id="ny-road-distance",graph_file="benchmarks/data/USA-road-d.NY.gr.gz",family="hot-pool",locality=70,interval=100,mode="increase-only",magnitude="large",budgets=(24*mib,)),
      dict(graph_id="syn-scale-free-50000-m4-v1",graph_file="syn-scale-free-50000-m4-v1.gr",family="hot-pool",locality=70,interval=10,mode="balanced-random",magnitude="small",budgets=(24*mib,)),
      dict(graph_id="syn-scale-free-50000-m4-v1",graph_file="syn-scale-free-50000-m4-v1.gr",family="hot-pool",locality=70,interval=100,mode="increase-only",magnitude="large",budgets=(24*mib,)),
    ]

def holdout3_cases():
    mib=1024*1024
    cases=[]
    graphs=(("syn-grid-224x224-v1","syn-grid-224x224-v1.gr"),("syn-uniform-50000-d6-v1","syn-uniform-50000-d6-v1.gr"),("syn-clustered-50000-c100-d6-v1","syn-clustered-50000-c100-d6-v1.gr"))
    families=(("zipf",75),("churn",90))
    scenarios=(("decrease-only","medium"),("strict-alternating","small"))
    for gid,gfile in graphs:
      for family,locality in families:
       for mode,mag in scenarios:
        for interval in (5,50):
         cases.append(dict(graph_id=gid,graph_file=gfile,family=family,locality=locality,interval=interval,mode=mode,magnitude=mag,budgets=(20*mib,40*mib)))
    return cases

def matrix(smoke,phase):
    if smoke:return {"cases":[dict(graph_id="syn-grid-224x224-v1",graph_file="syn-grid-224x224-v1.gr",family="churn",locality=100,interval=5,mode="strict-alternating",magnitude="medium",budgets=(16*1024*1024,))],"seeds":(211,),"repeats":(0,)}
    if phase=="regression":return {"cases":regression_cases(),"seeds":(7,17,29),"repeats":(0,)}
    return {"cases":holdout3_cases(),"seeds":(211,257,307),"repeats":(0,)}

def ensure_graph(repo,generated,gid,declared):
    if gid=="ny-road-distance": return repo/declared
    p=generated/declared
    if not p.is_file(): run_checked([sys.executable,str(repo/"tools/generate_synthetic_graphs.py"),str(generated),"--only",gid])
    return p

def run_phase(repo,build,out,phase,queries,smoke):
    spec=matrix(smoke,phase); raw=out/"raw"; traces=out/"traces"; oracles=out/"oracles"; generated=out/"generated-graphs"
    for p in (raw,traces,oracles,generated):p.mkdir(parents=True,exist_ok=True)
    generator=build/"ades_workload_v2"; runner=build/"ades_pr52_profile"
    if not generator.is_file() or not runner.is_file():raise RuntimeError("PR52 binaries not built")
    graph_paths={}
    for c in spec["cases"]:
      graph_paths.setdefault(c["graph_id"],ensure_graph(repo,generated,c["graph_id"],c["graph_file"]))
    hashes={gid:sha256_file(p) for gid,p in graph_paths.items()}
    rows=[]; seen=set(); trace_index=0
    for ci,c in enumerate(spec["cases"]):
      gp=graph_paths[c["graph_id"]]
      for seed in spec["seeds"]:
       trace_index+=1; stem=f"{phase}__{ci:03d}__{c['graph_id']}__{c['family']}__l{c['locality']}__u{c['interval']}__{c['mode']}__{c['magnitude']}__s{seed}"
       tr=traces/f"{stem}.trace"; meta=traces/f"{stem}.json"; oracle=oracles/f"{stem}.oracle"
       run_checked([str(generator),str(gp),str(tr),str(meta),str(seed),str(queries),c["family"],str(c["interval"]),c["mode"],c["magnitude"],"4","15",str(c["locality"]),"4"])
       run_checked([str(runner),"oracle",str(gp),str(tr),str(oracle)])
       md=json.loads(meta.read_text()); sha=md["trace_sha256"]
       if sha in seen:raise RuntimeError("duplicate PR52 trace SHA")
       seen.add(sha)
       for bi,budget in enumerate(c["budgets"]):
        for rep in spec["repeats"]:
         rotation=(trace_index+bi+rep)%len(PROFILES); order=PROFILES[rotation:]+PROFILES[:rotation]
         for oi,profile in enumerate(order):
          rss=raw/f"{stem}__b{budget}__{profile}.rss"; started=time.monotonic()
          p=subprocess.run(["/usr/bin/time","-f","%M","-o",str(rss),str(runner),"run",str(gp),str(tr),str(oracle),str(budget),profile],text=True,capture_output=True,check=False)
          wall=time.monotonic()-started
          if p.returncode:raise RuntimeError(f"profile failed {stem} {profile}: {p.stderr[-4000:]}")
          parsed=parse_profile(p.stdout)
          if parsed["trace_sha256"]!=sha or int(parsed["budget_bytes"])!=budget:raise RuntimeError("PR52 identity mismatch")
          if int(parsed["peak_accounted_bytes"])>budget:raise RuntimeError("PR52 byte-budget violation")
          rows.append(dict(phase=phase,graph_id=c["graph_id"],graph_sha256=hashes[c["graph_id"]],family=c["family"],locality_percent=c["locality"],update_interval=c["interval"],update_mode=c["mode"],magnitude=c["magnitude"],seed=seed,repeat=rep,execution_order=oi,**parsed,peak_rss_kb=rss.read_text().strip(),wall_s=f"{wall:.6f}"))
    return rows,spec,hashes

def write_csv(rows,path,fields=None):
    fields=fields or (list(rows[0]) if rows else [])
    with path.open("w",newline="") as f:
      w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(rows)

def median_int(vals):return int(statistics.median(int(v) for v in vals))
def summarize(rows):
    groups={}
    for r in rows:
      key=(r["phase"],r["graph_id"],r["family"],r["locality_percent"],r["update_interval"],r["update_mode"],r["magnitude"],r["budget_bytes"],r["profile"]); groups.setdefault(key,[]).append(r)
    out=[]
    for key,g in sorted(groups.items()):
      phase,graph,family,loc,interval,mode,mag,budget,profile=key
      out.append(dict(phase=phase,graph_id=graph,family=family,locality_percent=loc,update_interval=interval,update_mode=mode,magnitude=mag,budget_bytes=budget,profile=profile,samples=len(g),median_algorithm_ns=median_int(x["algorithm_ns"] for x in g),median_query_ns=median_int(x["query_ns"] for x in g),median_update_ns=median_int(x["update_ns"] for x in g),median_peak_rss_kb=median_int(x["peak_rss_kb"] for x in g),median_peak_accounted_bytes=median_int(x["peak_accounted_bytes"] for x in g),median_progressive_queries=median_int(x["progressive_queries"] for x in g),median_partial_creations=median_int(x["partial_creations"] for x in g),median_partial_invalidations=median_int(x["partial_invalidations"] for x in g),median_progressive_promotions=median_int(x["progressive_promotions"] for x in g)))
    return out

def confidence(rows):
    cells={}
    for r in rows:
      key=(r["phase"],r["graph_id"],r["family"],int(r["locality_percent"]),int(r["update_interval"]),r["update_mode"],r["magnitude"],int(r["budget_bytes"]),int(r["seed"]),int(r["repeat"])); cells.setdefault(key,{})[r["profile"]]=r
    pairs={}
    for cell,p in cells.items():
      if "ADES-V4" not in p:continue
      v4=int(p["ADES-V4"]["algorithm_ns"])
      for comp in COMPARATORS:
       if comp in p:pairs.setdefault((*cell[:-2],comp),[]).append((cell[-2],int(p[comp]["algorithm_ns"])/v4))
    out=[]
    for key,vals in sorted(pairs.items()):
      phase,graph,family,loc,interval,mode,mag,budget,comp=key; logs=[math.log(v) for _,v in vals]; mean=statistics.mean(logs); gm=math.exp(mean)
      if len(logs)>=2:
       sd=statistics.stdev(logs); half=(T95_DF2 if len(logs)==3 else 1.96)*sd/math.sqrt(len(logs)); lo,hi=math.exp(mean-half),math.exp(mean+half)
      else:lo=hi=float("nan")
      out.append(dict(phase=phase,graph_id=graph,family=family,locality_percent=loc,update_interval=interval,update_mode=mode,magnitude=mag,budget_bytes=budget,comparator=comp,paired_samples=len(vals),geomean_comparator_over_v4=f"{gm:.9f}",ci95_low="NA" if math.isnan(lo) else f"{lo:.9f}",ci95_high="NA" if math.isnan(hi) else f"{hi:.9f}"))
    return out

def evaluate(summary,conf):
    regimes={}
    for r in summary:
      key=(r["phase"],r["graph_id"],r["family"],r["locality_percent"],r["update_interval"],r["update_mode"],r["magnitude"],r["budget_bytes"]); regimes.setdefault(key,{})[r["profile"]]=int(r["median_algorithm_ns"])
    counts={}; near={}; totals={}
    for key,p in regimes.items():
      phase=key[0]; totals[phase]=totals.get(phase,0)+1; fastest=min(p.values()); winner=min(p,key=p.get); counts[f"{phase}:{winner}"]=counts.get(f"{phase}:{winner}",0)+1
      if p.get("ADES-V4",2**63)<=math.ceil(fastest*1.01):near[phase]=near.get(phase,0)+1
    regress={"regression":0,"holdout3":0}; wins={"regression":0,"holdout3":0}
    for r in conf:
      gm=float(r["geomean_comparator_over_v4"]); lo=None if r["ci95_low"]=="NA" else float(r["ci95_low"]); hi=None if r["ci95_high"]=="NA" else float(r["ci95_high"])
      if gm<=1/1.10 and hi is not None and hi<1:regress[r["phase"]]+=1
      if gm>=1.10 and lo is not None and lo>1:wins[r["phase"]]+=1
    return dict(regression_regimes=totals.get("regression",0),holdout3_regimes=totals.get("holdout3",0),regression_v4_fastest_or_within_1pct=near.get("regression",0),holdout3_v4_fastest_or_within_1pct=near.get("holdout3",0),regression_material_regressions=regress["regression"],holdout3_material_regressions=regress["holdout3"],regression_material_wins=wins["regression"],holdout3_material_wins=wins["holdout3"],holdout3_zero_material_regressions=regress["holdout3"]==0,fastest_profile_counts=counts)
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--repo",type=Path,default=Path(".")); ap.add_argument("--build",type=Path,default=Path("build")); ap.add_argument("--output",type=Path,default=Path("results/pr52-progressive")); ap.add_argument("--queries",type=int,default=160); ap.add_argument("--smoke",action="store_true"); ap.add_argument("--phase",choices=("regression","holdout3","both"),default="both"); a=ap.parse_args()
    repo=a.repo.resolve(); build=(repo/a.build).resolve() if not a.build.is_absolute() else a.build; out=(repo/a.output).resolve() if not a.output.is_absolute() else a.output; out.mkdir(parents=True,exist_ok=True)
    phases=["regression"] if a.smoke else ([a.phase] if a.phase!="both" else ["regression","holdout3"]); rows=[]; specs={}; hashes={}
    for phase in phases:
      rr,s,h=run_phase(repo,build,out/phase,phase,min(a.queries,16) if a.smoke else a.queries,a.smoke); rows+=rr; specs[phase]={"cases":[{**c,"budgets":list(c["budgets"])} for c in s["cases"]],"seeds":list(s["seeds"]),"repeats":list(s["repeats"])}; hashes[phase]=h
    write_csv(rows,out/"measurements.csv",MEASUREMENT_FIELDS); summary=summarize(rows); write_csv(summary,out/"summary.csv"); conf=confidence(rows); write_csv(conf,out/"confidence.csv"); result=evaluate(summary,conf); (out/"decision.json").write_text(json.dumps(result,indent=2)+"\n")
    manifest={"schema":"ades-pr52-progressive-v1","commit":os.environ.get("GITHUB_SHA","local-unpinned"),"claim_ids":["C1","C2","C3","C4","C8","C9"],"profiles":list(PROFILES),"regression_is_tuning_evidence":True,"holdout3_is_fresh_evidence":not a.smoke and "holdout3" in phases,"queries_per_trace":min(a.queries,16) if a.smoke else a.queries,"matrices":specs,"graph_sha256":hashes,"statistics":"paired comparator/ADES-V4 speedups; 95% t interval across 3 independent seeds","material_threshold":"10% point estimate plus CI entirely on comparator-winning/losing side of 1.0","result":result}; (out/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
    lines=["# PR52 Progressive Frontier Analysis","","- Regression cells are consumed development evidence only.","- Holdout3 is frozen fresh evidence.","",f"- holdout3 zero material regressions: {result['holdout3_zero_material_regressions']}",f"- holdout3 V4 fastest/within 1%: {result['holdout3_v4_fastest_or_within_1pct']} / {result['holdout3_regimes']}",f"- holdout3 material regressions: {result['holdout3_material_regressions']}",f"- holdout3 material wins: {result['holdout3_material_wins']}","","## Fastest counts"]+[f"- {k}: {v}" for k,v in sorted(result["fastest_profile_counts"].items())]; (out/"ANALYSIS.md").write_text("\n".join(lines)+"\n")
    print(json.dumps(result,indent=2)); return 0
if __name__=="__main__":raise SystemExit(main())
