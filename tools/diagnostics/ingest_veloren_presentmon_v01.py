#!/usr/bin/env python3
"""Idempotent native-only Veloren PresentMon A-side ingest. No Underdog speedup claims."""
import hashlib,json,pathlib,sqlite3
from datetime import datetime,timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
E=ROOT/"06_EVIDENCE"
RUN=ROOT/"07_EXECUTION"/"DX12_BINDING_TEXTURE_V01"
DB=ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
DATA=ROOT/"03_DATASET"/"DX12_FEATURE_MATRIX_V01.json"
KNOW=ROOT/"04_KNOWLEDGE"/"VELOREN_NATIVE_PRESENTMON_V01.md"
LEDGER=ROOT/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
RECEIPTS=[("VELOREN-NATIVE-UI-PM260-V01",E/"VELOREN_NATIVE_DX12_PRESENTMON_MENU_V01.json",RUN/"veloren_native_dx12_menu_presentmon.csv","UI_MENU_SCREEN_UNPINNED"),
 ("VELOREN-NATIVE-WORLD-PM260-V01",E/"VELOREN_NATIVE_DX12_PRESENTMON_WORLD_V01.json",RUN/"veloren_native_dx12_world_35s_presentmon.csv","IN_WORLD_FOREST_VISUAL_WITNESS")]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
dataset=json.loads(DATA.read_text(encoding="utf-8"))
rows=[]
for runid,receipt_path,raw_path,scene_kind in RECEIPTS:
    r=json.loads(receipt_path.read_text(encoding="utf-8"))
    assert r["schema"]=="underdog.presentmon.native_capture.v1"
    assert r["execution"]=="NATIVE_GAME_NO_UNDERDOG_ADAPTER"
    assert r["backend"]=="Dx12" and r["hardware_id"]=="host02-intel-hd-520"
    assert r["driver"]=="22.20.16.4836" and r["game_revision"]=="da6626e6"
    assert r["target_rows"]>=120 and r["present_interval"]["samples"]>=120
    assert r["csv_sha256"]==digest(raw_path)
    if scene_kind=="IN_WORLD_FOREST_VISUAL_WITNESS":
        visual=RUN/"native_perf_world_scene.png"
        assert visual.exists()
        image_sha=digest(visual)
        r["scene_image_sha256"]=image_sha
        r["scene_image_path"]=str(visual)
    else:
        image_sha=None
        r["scene"]="veloren_menu_ui_12s_state_unpinned"
        r["scene_classification_note"]="Menu-level UI; exact screen during timed capture not independently pinned."
    r["scene_gate"]=scene_kind
    rows.append((runid,r,receipt_path,raw_path,image_sha))
stamp=datetime.now(timezone.utc).isoformat()
with sqlite3.connect(DB) as conn:
    conn.execute("PRAGMA foreign_keys=ON")
    assert conn.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    assert conn.execute("SELECT status,underdog_integration FROM game_compatibility WHERE game_id='veloren' AND hardware_id='host02-intel-hd-520'").fetchone()==("in_progress","not_integrated")
    conn.execute("""CREATE TABLE IF NOT EXISTS game_performance_runs (
       run_id TEXT PRIMARY KEY, game_id TEXT NOT NULL, hardware_id TEXT NOT NULL,
       phase TEXT NOT NULL, graphics_api TEXT NOT NULL, scene_kind TEXT NOT NULL,
       raw_sha256 TEXT NOT NULL, presents INTEGER NOT NULL,
       average_present_fps REAL NOT NULL, interval_p50_ms REAL NOT NULL,
       interval_p95_ms REAL NOT NULL, gpu_time_p50_ms REAL,
       image_sha256 TEXT, evidence_path TEXT NOT NULL, qualified_utc TEXT NOT NULL,
       FOREIGN KEY(game_id,hardware_id) REFERENCES game_compatibility(game_id,hardware_id))""")
    for runid,r,receipt_path,raw_path,image_sha in rows:
        old=conn.execute("SELECT raw_sha256 FROM game_performance_runs WHERE run_id=?",(runid,)).fetchone()
        if old and old[0]!=r["csv_sha256"]:raise RuntimeError("Immutable native performance record conflict "+runid)
        conn.execute("""INSERT OR IGNORE INTO game_performance_runs VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)""",
            (runid,"veloren","host02-intel-hd-520","NATIVE_UNMODIFIED","D3D12",
             r["scene_gate"],r["csv_sha256"],r["target_rows"],r["aggregate_present_fps"],
             r["present_interval"]["p50_ms"],r["present_interval"]["p95_ms"],
             r["gpu_time"]["p50_ms"],image_sha,str(receipt_path),stamp))
        artifact_id=hashlib.sha256(str(raw_path).encode()).hexdigest()[:24]
        conn.execute("""INSERT OR IGNORE INTO artifacts
          (artifact_id,role,absolute_path,extension,bytes_on_source_disk,present,origin_workspace,indexed_utc)
          VALUES(?,?,?,?,?,?,?,?)""",(artifact_id,"NATIVE_PRESENTMON_CSV",str(raw_path),".csv",
             raw_path.stat().st_size,1,str(RUN),stamp))
    assert conn.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    conn.commit()
    perf_count=conn.execute("SELECT COUNT(*) FROM game_performance_runs").fetchone()[0]
for runid,r,path,raw,image_sha in rows:
    path.write_text(json.dumps(r,indent=2)+"\n",encoding="utf-8")
dataset["veloren_native_presentmon_baselines"]=[{
    "scene":r["scene"],"gate":r["scene_gate"],"captured_frames":r["target_rows"],
    "avg_present_fps":r["aggregate_present_fps"],"present_p50_ms":r["present_interval"]["p50_ms"],
    "present_p95_ms":r["present_interval"]["p95_ms"],"gpu_p50_ms":r["gpu_time"]["p50_ms"],
    "evidence":str(path)} for _,r,path,_,_ in rows]
t=DATA.with_name(DATA.name+".tmp")
t.write_text(json.dumps(dataset,indent=2)+"\n",encoding="utf-8");t.replace(DATA)
menu=rows[0][1];world=rows[1][1]
KNOW.write_text("# Veloren native D3D12 PresentMon V01\n\n"+
 "Hardware Host02 Intel HD 520, driver 22.20.16.4836, Win10 build 18362, Veloren revision da6626e6. WGPU_BACKEND=dx12. No Underdog code loaded.\n\n"+
 "UI phase 12s: 354 presentations; approx %.2f presented FPS; interval p50 %.3fms/p95 %.3fms; GPU time p50 %.3fms. Exact menu sub-screen not pinned during capture.\n\n"%(menu["aggregate_present_fps"],menu["present_interval"]["p50_ms"],menu["present_interval"]["p95_ms"],menu["gpu_time"]["p50_ms"])+
 "In-world idle forest 35s: 209 presentations; approx %.2f presented FPS; interval p50 %.3fms/p95 %.3fms; GPU time p50 %.3fms. Screenshot witnesses character/terrain/HUD in forest.\n\n"%(world["aggregate_present_fps"],world["present_interval"]["p50_ms"],world["present_interval"]["p95_ms"],world["gpu_time"]["p50_ms"])+
 "The scene appears GPU-bound, but driver ETW timing may have uncertainty. Changes in weather, lighting, camera and caching prevent automatic causal comparisons across different sessions.\n\n"+
 "Source CSVs retained, hashed and indexed. PresentMon 2.6.0 console was downloaded as a signed standalone binary; no MSI installed. These captures are only native A-side controls. An Underdog performance improvement is unmeasured until an audited Veloren build passes compile, playtest, image-parity and paired repeated A/B gates.\n",encoding="utf-8")
evs=[json.loads(x) for x in LEDGER.read_text(encoding="utf-8").splitlines() if x.strip()]
id="VELOREN-NATIVE-DX12-PRESENTMON-V01"
if not any(e.get("id")==id for e in evs):
    with LEDGER.open("a",encoding="utf-8") as f:f.write(json.dumps({"id":id,"utc":stamp,"claim":"native forest 209 presents in 35s, p50 GPU 164.24ms; no Underdog involvement","classification":"NATIVE_PERFORMANCE_BASELINE","evidence":str(rows[1][2]),"confidence":"hardware_presentmon_etw"})+"\n")
print(json.dumps({"status":"PRESENTMON_NATIVE_INGEST_OK","run_rows":perf_count,
  "menu_frames":menu["target_rows"],"world_frames":world["target_rows"],
  "world_fps":world["aggregate_present_fps"],"knowledge":str(KNOW)},indent=2))
