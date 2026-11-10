#!/usr/bin/env python3
"""Governed cleanup of regenerable source-inspection scratch only, never game/player data."""
import argparse,hashlib,json,pathlib,shutil,subprocess
from datetime import datetime,timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
TRACE=ROOT/"07_EXECUTION"/"VELOREN_TERRAIN_TRACE_V01"
RECEIPT=TRACE/"CLEANUP_TERRAIN_TRACE_V01.json"
KEEP=[
 TRACE/"upstream_terrain_da6626e6.rs",
 TRACE/"modified_terrain_da6626e6.rs",
 TRACE/"veloren_da6626e6_terrain_trace_v01.patch",
 TRACE/"generate_terrain_trace_patch_v01.py",
 TRACE/"validate_terrain_patch_v01.py",
 TRACE/"terrain_trace_patch_receipt_v01.json",
 TRACE/"unchanged_native_log_trace_parse_v01.json",
 ROOT/"06_EVIDENCE"/"VELOREN_TERRAIN_SOURCE_TRACE_V01.json",
 ROOT/"03_DATASET"/"VELOREN_TERRAIN_LIFECYCLE_V01.json",
 ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite",
 ROOT/"04_KNOWLEDGE"/"VELOREN_TERRAIN_TRACE_FINDINGS_V01.md",
 ROOT/"09_REPOSITORY"/"underdog-d3d-compat"/"tools"/"diagnostics"/"analyze_veloren_terrain_trace_v01.py",
]
SCRATCH=[
 TRACE/"inspect_endings.py",
 TRACE/"refine_terrain_patch_v02.py",
]
STAGED=TRACE/"apply_check"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--apply",action="store_true")
    args=ap.parse_args()
    for p in KEEP:
        if not p.is_file():raise RuntimeError("Missing durable input "+str(p))
    t=subprocess.run(["tasklist","/FI","IMAGENAME eq veloren-voxygen.exe"],capture_output=True,text=True)
    if t.returncode or "veloren-voxygen.exe" in t.stdout.lower():
        raise RuntimeError("Game active or cannot verify closure; stop cleanup")
    candidates=[]
    for p in SCRATCH:
        if p.is_file():candidates.append(p)
    if STAGED.exists():
        files=[x for x in STAGED.rglob("*") if x.is_file()]
        expected=STAGED/"voxygen"/"src"/"scene"/"terrain"/"mod.rs"
        if set(files)!={expected}:raise RuntimeError("Unexpected staged files: refuse recursive cleanup")
        candidates.extend(files)
    records=[{"path":str(p),"size":p.stat().st_size,"sha256":sha(p)} for p in candidates]
    receipt={"schema":"aios.underdog.veloren.terrain_trace_cleanup.v1",
       "utc":datetime.now(timezone.utc).isoformat(),
       "mode":"APPLIED" if args.apply else "DRY_RUN",
       "candidates":records,"potential_bytes":sum(x["size"] for x in records),
       "protected_sha256":{str(p):sha(p) for p in KEEP},
       "protected_player_world":"C:\\Games\\Veloren\\BASELINE_2026-10-29\\game\\userdata",
       "protected_copied_world":str(ROOT/"07_EXECUTION"/"VELOREN_PERF_TRIAGE_V01"/"userdata_scale065"),
       "patch_reconstruction":"source+generator+patch+validator retained"}
    if args.apply:
        for p in candidates:p.unlink()
        if STAGED.exists():shutil.rmtree(STAGED)
        RECEIPT.write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"mode":receipt["mode"],"files":len(records),
       "regenerable_bytes":receipt["potential_bytes"],"receipt":str(RECEIPT) if args.apply else None}))
if __name__=="__main__":main()
