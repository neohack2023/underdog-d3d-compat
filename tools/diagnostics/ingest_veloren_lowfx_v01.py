#!/usr/bin/env python3
"""Admit native low-effects Veloren result with runtime setting reconciliation and unmatched-scene warning."""
import hashlib,json,pathlib,shutil,sqlite3
from datetime import datetime,timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
RUN=ROOT/"07_EXECUTION"/"VELOREN_PERF_TRIAGE_V01"
VAR=RUN/"EFFECTS_VARIANT_RECEIPT_V01.json"
RAW=RUN/"veloren_lowfx_world35s_presentmon.csv"
SUMMARY=RUN/"presentmon_lowfx_summary.json"
SCREEN=RUN/"lowfx_ingame.png"
CFG=RUN/"userdata_scale065"/"voxygen"/"settings.ron"
BASE=pathlib.Path(r"C:\Games\Veloren\BASELINE_2026-10-29\game\userdata\voxygen\settings.ron")
SAVED=RUN/"settings_lowfx_after_run.ron"
OUT=ROOT/"06_EVIDENCE"/"VELOREN_NATIVE_DX12_LOWFX065_V01.json"
DATA=ROOT/"03_DATASET"/"VELOREN_GRAPHICS_AB_EXPERIMENT_V01.json"
DB=ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
NOTE=ROOT/"04_KNOWLEDGE"/"VELOREN_RENDER_SCALE065_V01.md"
LEDGER=ROOT/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
UTC=datetime.now(timezone.utc).isoformat()
def sha(f):return hashlib.sha256(f.read_bytes()).hexdigest()
for f in (VAR,RAW,SUMMARY,SCREEN,CFG,BASE,DATA,DB,NOTE):
    if not f.is_file():raise FileNotFoundError(f)
var=json.loads(VAR.read_text(encoding="utf-8"))
p=json.loads(SUMMARY.read_text(encoding="utf-8"))
mat=json.loads(DATA.read_text(encoding="utf-8"))
assert p["schema"]=="underdog.presentmon.native_capture.v1"
assert p["execution"]=="NATIVE_GAME_NO_UNDERDOG_ADAPTER" and p["target_rows"]>=120
assert p["backend"]=="Dx12" and p["hardware_id"]=="host02-intel-hd-520"
assert p["csv_sha256"]==sha(RAW)
assert sha(BASE)=="845a6d4f90276eb79e4f58e6a8c572ede8893e19c462664686d0a8bdd9df3a76"
settings=CFG.read_text(encoding="utf-8")
tokens=("cloud: Low,","fluid: Medium,","reflection: High,","shadow: r#None,",
        "bloom: Off,","point_glow: 0.0,","factor: 0.65,")
for token in tokens:
    assert token in settings,("runtime settings did not match expected",token)
if SAVED.exists():
    if sha(SAVED)!=sha(CFG):raise RuntimeError("Frozen lowfx setting snapshot mismatch")
else:shutil.copy2(CFG,SAVED)
rec=dict(p)
rec.update({"schema":"aios.underdog.veloren.native_effects_control.v1",
    "trial_id":"VELOREN-NATIVE-DX12-LOWFX065-V01",
    "captured_utc":UTC,"scene_gate":"IN_WORLD_VILLAGE_LOW_EFFECTS_EXPLORATORY",
    "render_scale":0.65,"changed_from_prior":"cloud High->Low, shadows Map->None, bloom On->Off, point_glow .35->0",
    "requested_but_not_applied":"fluid Medium->Cheap; game wrote Medium instead",
    "unchanged_from_prior":"fluid Medium, reflection High, view distances; resolution 1920x1080",
    "actual_settings_path":str(SAVED),"actual_settings_sha256":sha(SAVED),
    "original_settings_sha256":sha(BASE),
    "scene_image_path":str(SCREEN),"scene_image_sha256":sha(SCREEN),
    "comparison_status":"EXPLORATORY_SCENE_UNMATCHED_NO_UNDERDOG_GAIN_CLAIM",
    "limits":"Multi-setting bundle, no matched camera/time/weather or repeated paired trials; not a fix for reported terrain void"})
with sqlite3.connect(DB) as conn:
    conn.execute("PRAGMA foreign_keys=ON")
    assert conn.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    pid=rec["trial_id"]
    old=conn.execute("SELECT raw_sha256 FROM game_performance_runs WHERE run_id=?",(pid,)).fetchone()
    if old and old[0]!=rec["csv_sha256"]:raise RuntimeError("Immutable performance run hash conflict")
    conn.execute("INSERT OR IGNORE INTO game_performance_runs VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
        (pid,"veloren","host02-intel-hd-520","NATIVE_SETTINGS_VARIANT","D3D12",
         rec["scene_gate"],rec["csv_sha256"],rec["target_rows"],
         rec["aggregate_present_fps"],rec["present_interval"]["p50_ms"],
         rec["present_interval"]["p95_ms"],rec["gpu_time"]["p50_ms"],
         rec["scene_image_sha256"],str(OUT),UTC))
    assert conn.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    conn.commit()
    rowcount=conn.execute("SELECT COUNT(*) FROM game_performance_runs").fetchone()[0]
rec["db_performance_rows"]=rowcount
OUT.write_text(json.dumps(rec,indent=2)+"\n",encoding="utf-8")
for item in mat["cases"]:
    if item["id"]=="C_NATIVE_LOW_EFFECTS":
        item["state"]="MEASURED_EXPLORATORY_NONMATCHED_SCENE"
        item["evidence_path"]=str(OUT)
        item["captured_presents"]=rec["target_rows"]
        item["avg_present_fps"]=rec["aggregate_present_fps"]
        item["actual_applied_settings"]={"cloud":"Low","fluid":"Medium","shadow":"None","bloom":"Off","point_glow":0,"render_scale":0.65}
        item["requested_not_applied"]="fluid Cheap"
mat["latest_native_effects_receipt"]=str(OUT)
tmp=DATA.with_suffix(".json.tmp")
tmp.write_text(json.dumps(mat,indent=2)+"\n",encoding="utf-8");tmp.replace(DATA)
text=NOTE.read_text(encoding="utf-8")
section="\n## Native low-effects test (exploratory, NOT matched-route)\n"
if section.strip() not in text:
    text+=section+f"On the same isolated world and 0.65 scale with reduced clouds, shadow None, bloom Off, point glow 0, 35-second PresentMon captured {rec['target_rows']} presents, mean {rec['aggregate_present_fps']:.2f} FPS, GPU p50 {rec['gpu_time']['p50_ms']:.2f} ms. Fluid 'Cheap' was requested but Veloren wrote back 'Medium'; actual tested mode is Medium. Weather/time/camera not frozen. No Underdog adapter or verified terrain-hole repair.\nEvidence: {OUT}\n"
    NOTE.write_text(text,encoding="utf-8")
event={"id":rec["trial_id"],"utc":UTC,"event":"native_effects_control",
       "status":"EXPLORATORY","fps":rec["aggregate_present_fps"],
       "evidence":str(OUT),"limits":"multi-effect change; scene unmatched; no game patch"}
old=[json.loads(t) for t in LEDGER.read_text(encoding="utf-8").splitlines() if t.strip()]
if not any(x.get("id")==event["id"] for x in old):
    with LEDGER.open("a",encoding="utf-8") as f:f.write(json.dumps(event)+"\n")
print(json.dumps({"status":"INGEST_LOWFX_EXPLORATORY_OK","frames":rec["target_rows"],
                  "fps":rec["aggregate_present_fps"],"db_rows":rowcount,"evidence":str(OUT)}))
