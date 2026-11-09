#!/usr/bin/env python3
"""Allowlisted cleanup, dry-run by default. Never touches immutable game or external workspace."""
import argparse
import hashlib
import json
from pathlib import Path
from datetime import datetime, timezone
ROOT=Path(__file__).resolve().parents[4]
RUN=ROOT/"07_EXECUTION"/"DX12_FEATURE_PROBE_V01"
RECEIPT=RUN/"CLEANUP_RECEIPT_V02.json"
KEEP=[ROOT/"03_DATASET"/"DX12_FEATURE_MATRIX_V01.json",
      ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite",
      ROOT/"06_EVIDENCE"/"DX12_HD520_FEATURE_PROBE_V01.json",
      ROOT/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_FINDINGS_V01.md",
      RUN/"raw_capabilities.json",
      RUN/"alu_stream_stdout.txt",
      RUN/"wave_sm61_stdout.txt",
      ROOT/"06_EVIDENCE"/"DX12_HD520_SM61_WAVE_V01.json",
      ROOT/"09_REPOSITORY"/"underdog-d3d-compat"/"tools"/"diagnostics"/"dx12_feature_probe_v01.cpp",
      ROOT/"09_REPOSITORY"/"underdog-d3d-compat"/"tools"/"diagnostics"/"build_dx12_feature_probe_v01.cmd"]
CANDIDATES=[RUN/"prepare_wave_handoff.py",
            RUN/"wave_sm61_dispatch_v01.obj",
            RUN/"dx12_feature_probe_v01.obj",
            RUN/"alu_stream_v01.obj",
            RUN/"inspect_rows.py",
            RUN/"patch_ingest_v01.py",
            ROOT/"07_EXECUTION"/"inspect_existing_schema.py"]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--apply",action="store_true",help="delete allowlisted regenerable files only")
    args=p.parse_args()
    for required in KEEP:
        if not required.is_file():raise RuntimeError("Retention gate missing "+str(required))
    canonical=ROOT.resolve()
    eligible=[]
    for item in CANDIDATES:
        path=item.resolve()
        if canonical not in path.parents:raise RuntimeError("Outside workspace")
        if path.is_file():
            eligible.append({"path":str(path),"bytes":path.stat().st_size,"sha256":sha(path)})
    result={"schema":"aios.underdog.feature_probe.cleanup.v1",
            "utc":datetime.now(timezone.utc).isoformat(),
            "mode":"APPLIED" if args.apply else "DRY_RUN",
            "preserved_sha256":{str(f):sha(f) for f in KEEP},
            "allowlisted":eligible,
            "potential_bytes":sum(x["bytes"] for x in eligible)}
    if args.apply:
        for item in eligible:Path(item["path"]).unlink()
        result["deleted_bytes"]=result["potential_bytes"]
        RECEIPT.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"mode":result["mode"],"file_count":len(eligible),
                      "bytes":result["potential_bytes"],"receipt":str(RECEIPT) if args.apply else None},indent=2))
if __name__=="__main__":main()
