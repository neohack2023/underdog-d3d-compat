#!/usr/bin/env python3
"""Modify only a dedicated VELOREN_USERDATA test profile; preserve original game settings."""
import argparse,hashlib,json,pathlib,re,shutil
from datetime import datetime,timezone
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--profile",required=True)
    p.add_argument("--proof-root",required=True)
    args=p.parse_args()
    profile=pathlib.Path(args.profile).resolve()
    proof=pathlib.Path(args.proof_root).resolve()
    expected=proof/"userdata_scale065"
    if profile!=expected:raise ValueError("This bounded experiment only accepts the known isolated profile")
    cfg=profile/"voxygen"/"settings.ron"
    first=profile/"VARIANT_RECEIPT_V01.json"
    if not cfg.exists() or not first.exists():raise FileNotFoundError("Cannot find isolated profile")
    prior=json.loads(first.read_text(encoding="utf-8"))
    if prior["only_intended_change"]!="upscale_mode.factor 1.0 -> 0.65":raise ValueError("Wrong baseline variant")
    snap=proof/"settings_scale065_pre_effects.ron"
    if snap.exists():raise FileExistsError("Snapshot already exists; not overwriting test history")
    original=cfg.read_text(encoding="utf-8")
    changes=[
      (r"\bcloud:\s*High,", "cloud: Low,"),
      (r"\bfluid:\s*Medium,", "fluid: Cheap,"),
      (r"\bshadow:\s*Map\(\(\s*resolution:\s*1\.0,\s*\)\),", "shadow: None,"),
      (r"\bbloom:\s*On\(\(\s*factor:\s*Medium,\s*uniform_blur:\s*false,\s*\)\),", "bloom: Off,"),
      (r"\bpoint_glow:\s*0\.35,", "point_glow: 0.0,"),
    ]
    amended=original
    for regex,repl in changes:
        amended,n=re.subn(regex,repl,amended)
        if n!=1:raise ValueError("Expected one exact render option: "+regex+" got "+str(n))
    if amended==original:raise AssertionError("No change")
    snap.write_text(original,encoding="utf-8")
    cfg.write_text(amended,encoding="utf-8")
    r={"schema":"aios.underdog.veloren.effects_variant.v1","created_utc":datetime.now(timezone.utc).isoformat(),
       "profile":str(profile),"settings_before_sha256":sha(snap),"settings_after_sha256":sha(cfg),
       "changes":["cloud High -> Low","fluid Medium -> Cheap","shadows Map -> None",
                  "bloom On -> Off","point glow .35 -> 0"],
       "internal_render_scale":"0.65 unchanged","original_game_userdata":"UNTOUCHED",
       "status":"PROFILE_STAGED_NOT_EXECUTED",
       "restore_file":str(snap),
       "evidence":"This is a multi-effect bundle, not a single-variable causal test"}
    (proof/"EFFECTS_VARIANT_RECEIPT_V01.json").write_text(json.dumps(r,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"status":"LOW_EFFECTS_STAGED","before":r["settings_before_sha256"],
                      "after":r["settings_after_sha256"],"changes":r["changes"]},indent=2))
if __name__=="__main__":main()
