#!/usr/bin/env python3
"""Ingest native Veloren isolated render-scale trial. Does not claim controlled A/B speedup."""
import hashlib,json,pathlib,sqlite3
from datetime import datetime,timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
RUN=ROOT/"07_EXECUTION"/"VELOREN_PERF_TRIAGE_V01"
VAR=RUN/"userdata_scale065"/"VARIANT_RECEIPT_V01.json"
RAW=RUN/"veloren_scale065_world35s_presentmon.csv"
SCREEN=RUN/"scale065_ingame.png"
SUMMARY=ROOT/"06_EVIDENCE"/"VELOREN_NATIVE_DX12_RENDER_SCALE065_V01.json"
PERF_SRC=RUN/"presentmon_scale065_summary.json"
DATA=ROOT/"03_DATASET"/"VELOREN_GRAPHICS_AB_EXPERIMENT_V01.json"
DB=ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
KNOW=ROOT/"04_KNOWLEDGE"/"VELOREN_RENDER_SCALE065_V01.md"
LEDGER=ROOT/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
ORIG=pathlib.Path(r"C:\Games\Veloren\BASELINE_2026-10-29\game\userdata\voxygen\settings.ron")
UTC=datetime.now(timezone.utc).isoformat()
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for p in (VAR,RAW,SCREEN,PERF_SRC,DATA,DB,ORIG):
    if not p.is_file():raise FileNotFoundError(p)
variant=json.loads(VAR.read_text(encoding="utf-8"))
perf=json.loads(PERF_SRC.read_text(encoding="utf-8"))
data=json.loads(DATA.read_text(encoding="utf-8"))
assert variant["schema"]=="aios.underdog.veloren.userdata_variant.v1"
assert variant["only_intended_change"]=="upscale_mode.factor 1.0 -> 0.65"
assert digest(ORIG)==variant["original_settings_sha256"]
assert perf["execution"]=="NATIVE_GAME_NO_UNDERDOG_ADAPTER"
assert perf["hardware_id"]=="host02-intel-hd-520"
assert perf["backend"]=="Dx12" and perf["game_revision"]=="da6626e6"
assert perf["target_rows"]>=120
assert perf["csv_sha256"]==digest(RAW)
assert perf["present_interval"]["samples"]>=120
receipt=dict(perf)
receipt.update({
 "schema":"aios.underdog.veloren.native_scale_control.v1",
 "captured_utc":UTC,
 "trial_id":"VELOREN-NATIVE-DX12-SCALE065-V01",
 "source_settings_sha256":variant["original_settings_sha256"],
 "variant_settings_sha256":variant["variant_settings_sha256"],
 "variant_profile_path":str(VAR.parent),
 "variant":"internal_render_scale_065_only",
 "scene_image_path":str(SCREEN),
 "scene_image_sha256":digest(SCREEN),
 "scene_gate":"IN_WORLD_VILLAGE_SINGLE_SCREENSHOT",
 "comparison_status":"EXPLORATORY_NONMATCHED_SCENE",
 "not_claimed":"No Underdog loaded; not paired route; cannot infer causal speedup versus earlier forest scene."
})
with sqlite3.connect(DB) as con:
    con.execute("PRAGMA foreign_keys=ON")
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    con.execute("""CREATE TABLE IF NOT EXISTS game_performance_runs (
       run_id TEXT PRIMARY KEY, game_id TEXT NOT NULL, hardware_id TEXT NOT NULL,
       phase TEXT NOT NULL, graphics_api TEXT NOT NULL, scene_kind TEXT NOT NULL,
       raw_sha256 TEXT NOT NULL, presents INTEGER NOT NULL, average_present_fps REAL NOT NULL,
       interval_p50_ms REAL NOT NULL, interval_p95_ms REAL NOT NULL, gpu_time_p50_ms REAL,
       image_sha256 TEXT, evidence_path TEXT NOT NULL, qualified_utc TEXT NOT NULL,
       FOREIGN KEY(game_id,hardware_id) REFERENCES game_compatibility(game_id,hardware_id))""")
    pid=receipt["trial_id"]
    prev=con.execute("SELECT raw_sha256 FROM game_performance_runs WHERE run_id=?",(pid,)).fetchone()
    if prev and prev[0]!=receipt["csv_sha256"]:
        raise RuntimeError("Immutable performance run ID collision")
    con.execute("INSERT OR IGNORE INTO game_performance_runs VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
       (pid,"veloren","host02-intel-hd-520","NATIVE_SETTINGS_VARIANT","D3D12",
        receipt["scene_gate"],receipt["csv_sha256"],receipt["target_rows"],
        receipt["aggregate_present_fps"],receipt["present_interval"]["p50_ms"],
        receipt["present_interval"]["p95_ms"],receipt["gpu_time"]["p50_ms"],
        receipt["scene_image_sha256"],str(SUMMARY),UTC))
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    con.commit()
    count=con.execute("SELECT COUNT(*) FROM game_performance_runs").fetchone()[0]
receipt["database_total_game_performance_rows"]=count
SUMMARY.write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
for item in data["cases"]:
    if item["id"]=="B_NATIVE_RENDER_SCALE":
        item["state"]="MEASURED_EXPLORATORY_NONMATCHED_SCENE"
        item["receipt_path"]=str(SUMMARY)
        item["avg_present_fps"]=receipt["aggregate_present_fps"]
        item["captured_presents"]=receipt["target_rows"]
data["latest_native_scale_control"]=str(SUMMARY)
tmp=DATA.with_suffix(".json.tmp")
tmp.write_text(json.dumps(data,indent=2)+"\n",encoding="utf-8")
tmp.replace(DATA)
KNOW.write_text("# Veloren isolated render-scale control V01\n\n"
    f"Native D3D12 game da6626e6, Intel HD520, driver 22.20.16.4836; "
    f"unchanged executable. Original userdata and settings hash preserved. "
    f"Temporary isolated VELOREN_USERDATA cloned 16 files / approximately 22.7 MB, "
    f"one intended config change: upscale_mode.factor 1.0 -> 0.65.\n\n"
    f"Captured {receipt['target_rows']} PresentMon intervals in an in-world village scene; "
    f"average presented FPS {receipt['aggregate_present_fps']:.2f}, "
    f"interval p50 {receipt['present_interval']['p50_ms']:.2f}ms, "
    f"p95 {receipt['present_interval']['p95_ms']:.2f}ms, "
    f"GPU p50 {receipt['gpu_time']['p50_ms']:.2f}ms.\n\n"
    "IMPORTANT: The previous 1.0-scale sample was an idle forest scene, not a camera-locked A/B. "
    "Treat differences as exploratory evidence, not demonstrated speedup. "
    "No terrain-hole screenshot or repro obtained. All variants native without Underdog.\n\n"
    f"Raw CSV: {RAW}\nScreenshot: {SCREEN}\nReceipt: {SUMMARY}\n",encoding="utf-8")
record={"id":"VELOREN-NATIVE-DX12-SCALE065-V01","utc":UTC,"event":"native_resolution_control",
        "classification":"EXPLORATORY_NONMATCHED_SCENE",
        "presents":receipt["target_rows"],"avg_present_fps":receipt["aggregate_present_fps"],
        "evidence":str(SUMMARY),"notes":"Not an Underdog gain or terrain-hole repair"}
prev=[json.loads(x) for x in LEDGER.read_text(encoding="utf-8").splitlines() if x.strip()] if LEDGER.exists() else []
if not any(x.get("id")==record["id"] for x in prev):
    with LEDGER.open("a",encoding="utf-8") as f:f.write(json.dumps(record)+"\n")
print(json.dumps({"status":"INGESTED_EXPLORATORY_CONTROL","presents":receipt["target_rows"],
                  "fps":receipt["aggregate_present_fps"],"db_rows":count,"evidence":str(SUMMARY)}))
