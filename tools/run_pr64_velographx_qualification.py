#!/usr/bin/env python3
import argparse, json, pathlib, subprocess, sys

PIN = "2aa84ce81a180487239c661b65639e5100136075"
CASES = ["simple","zero_equal","asymmetric","nonzero_source","removal","random_mixed","independent_sources"]

def main():
    p=argparse.ArgumentParser(); p.add_argument("--probe",required=True); p.add_argument("--out",required=True); p.add_argument("--timeout",type=float,default=15.0); a=p.parse_args()
    out=pathlib.Path(a.out); out.mkdir(parents=True,exist_ok=True)
    results=[]
    for case in CASES:
        try:
            r=subprocess.run([a.probe,case],text=True,capture_output=True,timeout=a.timeout)
            status="PASS" if r.returncode==0 else "MISMATCH"
            row={"case":case,"status":status,"returncode":r.returncode,"stdout":r.stdout,"stderr":r.stderr}
        except subprocess.TimeoutExpired as e:
            row={"case":case,"status":"TIMEOUT","returncode":None,"stdout":e.stdout or "","stderr":e.stderr or ""}
        results.append(row); (out/f"{case}.json").write_text(json.dumps(row,indent=2,sort_keys=True)+"\n")
    semantic_pass=all(x["status"]=="PASS" for x in results)
    summary={
      "schema":"ades-pr64-velographx-weighted-sssp-qualification-v1",
      "upstream_repository":"sauravsingla/VeloGraphX",
      "upstream_commit":PIN,
      "upstream_algorithm_modified":False,
      "cases":results,
      "core_semantics_pass":semantic_pass,
      "source_audit":{
        "exact_weighted_sssp":True,
        "directed_graphs":True,
        "nonnegative_uint64_weights":True,
        "weight_decreases":"localized relaxation",
        "weight_increases":"full exact recomputation",
        "edge_removals":"full exact recomputation",
        "fixed_source_state":True,
        "arbitrary_source_adapter":"possible only via separately owned graph+SSSP instances or fresh reconstruction; all retained state must be charged",
        "simple_graph_semantics":True,
        "parallel_edge_identity":"not representable by WeightedDynamicGraph endpoint-keyed map",
        "license":"Apache-2.0"
      },
      "classification":"PENDING_ADAPTER_MEMORY_TIMING_AND_TRACE_QUALIFICATION" if semantic_pass else "INELIGIBLE_CORE_SEMANTICS",
      "next_gate":"Before DIRECT classification, prove exact PR42 canonical trace representability, reject duplicate-endpoint ambiguity, implement arbitrary-source admission/eviction with full persistent-state accounting, declare creation/recompute timing, and rerun exactness against ADES_ORACLE_V2.",
      "claim_boundary":"No performance result is implied by semantic qualification."
    }
    (out/"summary.json").write_text(json.dumps(summary,indent=2,sort_keys=True)+"\n")
    print(json.dumps(summary,indent=2,sort_keys=True))
    return 0 if semantic_pass else 3

if __name__=="__main__":
    try: raise SystemExit(main())
    except Exception as e:
        print(f"VeloGraphX qualification harness failure: {e}",file=sys.stderr); raise
