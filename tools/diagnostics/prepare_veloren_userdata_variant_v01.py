#!/usr/bin/env python3
"""Isolated Veloren single-variable graphics experiment: copy userdata, leave source unmodified."""
import argparse, hashlib, json, pathlib, re, shutil, sys
from datetime import datetime, timezone
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--source",required=True)
    ap.add_argument("--target",required=True)
    ap.add_argument("--scale",type=float,required=True)
    a=ap.parse_args()
    src=pathlib.Path(a.source).resolve()
    dst=pathlib.Path(a.target).resolve()
    if not (0.5<=a.scale<1.0):raise ValueError("Conservative experiment range is 0.5 <= scale < 1.0")
    if not src.is_dir():raise FileNotFoundError(src)
    orig=src/"voxygen"/"settings.ron"
    if not orig.is_file():raise FileNotFoundError(orig)
    if dst.exists():raise FileExistsError("Refuse existing target; no overwrite: "+str(dst))
    if src in dst.parents or dst in src.parents:raise ValueError("Nested source/target prohibited")
    src_hash=sha(orig)
    base_text=orig.read_text(encoding="utf-8")
    pattern=r"(upscale_mode:\s*\(\s*factor:\s*)1\.0(,)"
    amended,n=re.subn(pattern,lambda m:m.group(1)+str(a.scale)+m.group(2),base_text)
    if n!=1:raise ValueError("Expected exactly one original render scale 1.0 in upscale_mode")
    shutil.copytree(src,dst,symlinks=False)
    copied=dst/"voxygen"/"settings.ron"
    if sha(copied)!=src_hash:raise AssertionError("Copy mismatch; no patch applied")
    copied.write_text(amended,encoding="utf-8")
    if sha(orig)!=src_hash:raise AssertionError("Original settings changed unexpectedly!")
    items=[p for p in dst.rglob("*") if p.is_file()]
    m={"schema":"aios.underdog.veloren.userdata_variant.v1",
       "created_utc":datetime.now(timezone.utc).isoformat(),
       "source":str(src),"target":str(dst),
       "original_settings_sha256":src_hash,
       "variant_settings_sha256":sha(copied),
       "only_intended_change":"upscale_mode.factor 1.0 -> "+str(a.scale),
       "original_source_unchanged":sha(orig)==src_hash,
       "copied_files":len(items),"copied_size_bytes":sum(p.stat().st_size for p in items),
       "status":"STAGED_NOT_EXECUTED",
       "game_executable":"SHARED_UNMODIFIED_INSTALL_VIA_VELOREN_USERDATA",
       "safeguard":"Delete isolated target only after test saves/logs and receipts have been harvested"}
    out=dst/"VARIANT_RECEIPT_V01.json"
    out.write_text(json.dumps(m,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"status":"ISOLATED_PROFILE_READY","target":str(dst),
                      "source_settings_unchanged":True,"files":len(items),
                      "bytes":m["copied_size_bytes"]},indent=2))
if __name__=="__main__":main()
