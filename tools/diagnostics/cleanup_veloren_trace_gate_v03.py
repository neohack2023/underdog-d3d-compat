#!/usr/bin/env python3
"""Remove only redundant small image thumbnails from native Veloren replay, protect originals."""
import argparse,hashlib,json,pathlib,subprocess
from datetime import datetime,timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
N=ROOT/"07_EXECUTION"/"VELOREN_TERRAIN_NATIVE_WGPU_TRACE_V01"
E=ROOT/"06_EVIDENCE"/"VELOREN_TERRAIN_BOUNDED_BUILD_V03.json"
DB=ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
B=ROOT/"07_EXECUTION"/"VELOREN_TERRAIN_BUILD_EVAL_V01"
RECEIPT=N/"CLEANUP_TRACE_GATE_V03.json"
NAMES=["native_gpu_trace_menu","native_gpu_trace_world_select",
 "native_gpu_trace_world_clicked","native_gpu_trace_after_world_enter",
 "native_gpu_trace_ingame","native_gpu_trace_post_w_9s",
 "native_gpu_trace_post_w_20s","native_gpu_trace_strfwd"]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser();ap.add_argument("--apply",action="store_true")
    args=ap.parse_args()
    if not N.is_dir():raise FileNotFoundError(N)
    for p in (E,DB,B/"BOUNDED_BUILD_V02.json",B/"source_sparse"/"Cargo.lock"):
        if not p.is_file():raise FileNotFoundError(p)
    for proc in ("veloren-voxygen.exe","cargo.exe"):
        t=subprocess.run(["tasklist","/FI","IMAGENAME eq "+proc],capture_output=True,text=True)
        if t.returncode or proc.lower() in t.stdout.lower():
            raise RuntimeError("Game or build running; refuse cleanup "+proc)
    c=[]
    protected={}
    for name in NAMES:
        original=N/(name+".png");thumb=N/(name+"_thumb.jpg")
        if not original.is_file() or original.is_symlink():
            raise RuntimeError("Missing original image for "+name)
        protected[name]=sha(original)
        if thumb.is_file():
            if thumb.is_symlink() or thumb.parent.resolve()!=N.resolve():
                raise RuntimeError("Unsafe thumb "+str(thumb))
            c.append({"path":str(thumb),"size":thumb.stat().st_size,"sha256":sha(thumb)})
    outcome={
        "schema":"aios.underdog.veloren.trace_gate_cleanup.v3",
        "utc":datetime.now(timezone.utc).isoformat(),
        "state":"APPLIED" if args.apply else "DRY_RUN",
        "bytes_reclaimed_or_candidate":sum(x["size"] for x in c),
        "deleted_or_candidate":c,
        "protected_full_size_images_sha256":protected,
        "protected_build_cache":str(B/"target"),
        "protected_compiler_output":str(B/"cargo_build_voxygen_v02_stderr.log"),
        "protected_world_fixture":str(ROOT/"07_EXECUTION"/"VELOREN_PERF_TRIAGE_V01"/"userdata_scale065"),
        "protected_executable":"C:\\Games\\Veloren\\BASELINE_2026-10-29\\game\\veloren-voxygen.exe",
        "protected_evidence":str(E),
        "rule":"Regenerable thumbnails only, no compiled objects or world/user saves removed"}
    if args.apply:
        if RECEIPT.exists():raise FileExistsError("Cleanup already receipted")
        for entry in c:pathlib.Path(entry["path"]).unlink()
        RECEIPT.write_text(json.dumps(outcome,indent=2)+"\n",encoding="utf-8")
        assert all(sha(N/(n+".png"))==protected[n] for n in NAMES)
    print(json.dumps({"status":outcome["state"],"files":len(c),
       "bytes":outcome["bytes_reclaimed_or_candidate"],
       "original_pngs_preserved":True,"build_cache_preserved":True},indent=2))
if __name__=="__main__":main()
