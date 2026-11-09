#!/usr/bin/env python3
"""Strict cleanup of tiny redundant Veloren triage byproducts. Never delete the test world."""
import argparse,hashlib,json,pathlib,subprocess
from datetime import datetime,timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
RUN=ROOT/"07_EXECUTION"/"VELOREN_PERF_TRIAGE_V01"
EVID=ROOT/"06_EVIDENCE"
RECEIPT=RUN/"CLEANUP_TRIAGE_V01.json"
KEEP={
 "generic_capture_script":RUN/"capture_iso.ps1",
 "original_scale_snapshot":RUN/"settings_scale065_pre_effects.ron",
 "observed_lowfx_snapshot":RUN/"settings_lowfx_after_run.ron",
 "scale_csv":RUN/"veloren_scale065_world35s_presentmon.csv",
 "lowfx_csv":RUN/"veloren_lowfx_world35s_presentmon.csv",
 "scale_image":RUN/"scale065_ingame.png",
 "lowfx_image":RUN/"lowfx_ingame.png",
 "scale_receipt":EVID/"VELOREN_NATIVE_DX12_RENDER_SCALE065_V01.json",
 "lowfx_receipt":EVID/"VELOREN_NATIVE_DX12_LOWFX065_V01.json",
 "defect_receipt":EVID/"VELOREN_TERRAIN_VOID_USER_REPORT_V01.json",
}
JPG_NAMES=(
 "scale065_afterplay_thumb.jpg",
 "scale065_ingame_thumb.jpg",
 "scale065_current_thumb.jpg",
 "lowfx_preentry_thumb.jpg",
 "lowfx_ingame_thumb.jpg",
)
STATIC_SCRIPTS=(
 "capture_iso_afterplay.ps1",
 "capture_iso_select.ps1",
 "capture_world_select.ps1",
 "click_iso.ps1",
)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--apply",action="store_true")
    args=p.parse_args()
    if not (RUN/"userdata_scale065").is_dir():
        raise RuntimeError("Isolated test fixture missing; do not cleanup until evidence inventory reconciled")
    for label,path in KEEP.items():
        if not path.is_file():raise RuntimeError("Missing protected evidence "+label+" "+str(path))
    # Never remove anything while game is running.
    t=subprocess.run(["tasklist","/FI","IMAGENAME eq veloren-voxygen.exe"],capture_output=True,text=True)
    if t.returncode!=0:raise RuntimeError("Unable to prove Veloren is closed")
    if "veloren-voxygen.exe" in t.stdout.lower():raise RuntimeError("Veloren process still running; abort")
    targets=[]
    for n in JPG_NAMES:
        orig=RUN/n.replace("_thumb.jpg",".png")
        if not orig.is_file():raise RuntimeError("No full-resolution source for "+n)
        targets.append(RUN/n)
    for n in STATIC_SCRIPTS:targets.append(RUN/n)
    allowed=[]
    for item in targets:
        if item.exists():
            if not item.is_file() or item.is_symlink() or item.parent.resolve()!=RUN.resolve():
                raise RuntimeError("Unsafe cleanup candidate "+str(item))
            allowed.append({"path":str(item),"sha256":sha(item),"bytes":item.stat().st_size})
    result={"schema":"aios.underdog.veloren.triage_cleanup.v1",
            "timestamp_utc":datetime.now(timezone.utc).isoformat(),
            "status":"APPLIED" if args.apply else "DRY_RUN",
            "deleted_or_candidate":allowed,"bytes":sum(x["bytes"] for x in allowed),
            "protected_evidence_sha256":{k:sha(v) for k,v in KEEP.items()},
            "protected_mutable_fixture":str(RUN/"userdata_scale065"),
            "fixture_policy":"22.7MB world/profile copy retained while terrain-void cause unresolved"}
    if args.apply:
        for row in allowed:pathlib.Path(row["path"]).unlink()
        RECEIPT.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"status":result["status"],"files":len(allowed),
                      "bytes":result["bytes"],
                      "protected_fixture_retained":True},indent=2))
if __name__=="__main__":main()
