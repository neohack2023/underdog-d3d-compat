#!/usr/bin/env python3
"""Capture user-observed Veloren terrain-hole issue without misclassifying cause."""
import hashlib, json, pathlib, sqlite3
from datetime import datetime, timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
DB=ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
DATA=ROOT/"03_DATASET"/"VELOREN_RUNTIME_DEFECTS_V01.json"
EVID=ROOT/"06_EVIDENCE"/"VELOREN_TERRAIN_VOID_USER_REPORT_V01.json"
LEDGER=ROOT/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
HANDOFF=ROOT/"04_KNOWLEDGE"/"VELOREN_TERRAIN_VOID_INVESTIGATION_V01.md"
REPO=ROOT/"09_REPOSITORY"/"underdog-d3d-compat"
SETTINGS=pathlib.Path(r"C:\Games\Veloren\BASELINE_2026-10-29\game\userdata\voxygen\settings.ron")
PERF=ROOT/"06_EVIDENCE"/"VELOREN_NATIVE_DX12_PRESENTMON_WORLD_V01.json"
BASELINE=ROOT/"07_EXECUTION"/"VELOREN_NATIVE_BASELINE_V01"/"VELOREN_NATIVE_BASELINE_V01.json"
UTC=datetime.now(timezone.utc).isoformat()
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for p in (DB,SETTINGS,PERF,BASELINE,LEDGER):
    if not p.is_file():raise FileNotFoundError(p)
perf=json.loads(PERF.read_text(encoding="utf-8"))
assert perf["execution"]=="NATIVE_GAME_NO_UNDERDOG_ADAPTER"
assert perf["hardware_id"]=="host02-intel-hd-520" and perf["backend"]=="Dx12"
assert perf["target_rows"]==209 and perf["present_interval"]["samples"]==209
knobs={
    "window_resolution":"1920x1080",
    "terrain_view_distance":10,"entity_view_distance":65,
    "lod_distance":200,"sprite_render_distance":100,
    "cloud":"High","reflection":"High","fluid":"Medium",
    "shadow":"Map(resolution=1.0)","bloom":"On(Medium)",
    "render_scale":1.0,"particles_enabled":True
}
s=SETTINGS.read_text(encoding="utf-8")
for token in ('terrain_view_distance: 10,','cloud: High,','reflection: High,',
              'fluid: Medium,','sprite_render_distance: 100,','factor: 1.0,',
              'bloom: On((','resolution: (1920, 1080),'):
    assert token in s,("settings drift",token)
assert digest(PERF)==digest(PERF)
record={
 "schema":"aios.underdog.veloren.user_reported_visual_defect.v1",
 "id":"VELOREN-HD520-TERRAIN-VOID-V01",
 "recorded_utc":UTC,
 "origin":"user_direct_observation_not_instrumented_reproduction",
 "user_description":"While playing Veloren on Host 02, significant lag and bugs; at one point a black hole of blocks appeared beneath the character.",
 "status":"OPEN_UNREPRODUCED",
 "severity":"GAMEPLAY_VISUAL_DEFECT",
 "game_revision":"da6626e6",
 "graphics_backend":"D3D12 (verified prior launches, exact user's bad-frame backend not independently traced)",
 "gpu":"Intel HD Graphics 520","driver":"22.20.16.4836",
 "native_baseline_path":str(BASELINE),
 "performance_reference_path":str(PERF),
 "reference_frames":perf["target_rows"],
 "reference_mean_present_fps":perf["aggregate_present_fps"],
 "reference_gpu_time_p50_ms":perf["gpu_time"]["p50_ms"],
 "settings_sha256":digest(SETTINGS),
 "settings_readonly_snapshot":knobs,
 "hole_screenshot":None,
 "hole_log_trace":None,
 "cause":"UNKNOWN",
 "hypotheses":[
   {"id":"CHUNK_STREAM_DELAY","status":"unverified","signal":"hole fills in after standing still"},
   {"id":"MESH_OR_CULL_FAILURE","status":"unverified","signal":"collision exists while visual geometry is absent"},
   {"id":"GPU_RESOURCE_LIFETIME","status":"unverified","signal":"renderer validation error, stale terrain texture or missing draw"},
   {"id":"WORLD_DATA_OR_GENERATION","status":"unverified","signal":"repeatable at the same chunk in the same saved world"},
   {"id":"DRIVER_BACKEND_DIFFERENCE","status":"unverified","signal":"repeatable on Dx12 but not on dx11 at equivalent settings"}
 ],
 "reproduction_plan":[
   "Keep all native files and saved single-player world unchanged; avoid driver changes and duplicate game copies",
   "Run same saved world, character and walking route with native Dx12 and capture screenshot/video/time, PresentMon and voxygen log",
   "When hole appears, stand still briefly, test whether terrain streams back; check collision vs visual absence without entering dangerous void",
   "Compare controlled native dx11 run after an equivalent settings snapshot; do not treat a driver switch as an Underdog improvement",
   "Change one graphics knob at a time: render scale, clouds/shadows/bloom, then terrain/sprite/LOD distances",
   "Add chunk-lifecycle and mesh-upload instrumentation to optional source-level audit if symptoms reproduce"
 ],
 "admission_rule":"User report admitted as symptom only; no confirmed driver defect, corrupted chunk, performance improvement or repair."
}
DATA.parent.mkdir(parents=True,exist_ok=True)
EVID.parent.mkdir(parents=True,exist_ok=True)
HANDOFF.parent.mkdir(parents=True,exist_ok=True)
with sqlite3.connect(DB) as con:
    con.execute("PRAGMA foreign_keys=ON")
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    existing=con.execute("SELECT evidence_path FROM defects WHERE defect_id=?",(record["id"],)).fetchone()
    if existing and existing[0]!=str(EVID):
        raise RuntimeError("Existing defect ID points elsewhere; no silent overwrite")
    con.execute("""INSERT OR IGNORE INTO defects
        (defect_id,subsystem,symptom,status,test_target,evidence_path,owner_scope)
        VALUES(?,?,?,?,?,?,?)""",
        (record["id"],"Veloren terrain chunk/render streaming on Intel HD 520",
         record["user_description"],"OPEN_UNREPRODUCED",
         "Pinned native Dx12 saved world, 35-second repro with capture and dx11 control",
         str(EVID),"D3D_COMPAT_UNDERDOG_01"))
    con.execute("INSERT OR IGNORE INTO observations VALUES(?,?,?,?,?,?,?,?)",
        (record["id"],"USER_REPORTED_VISUAL_DEFECT",record["user_description"],
         str(EVID),"UNVERIFIED_CAUSE",0,str(DB),UTC))
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    con.commit()
    counts={t:con.execute("SELECT COUNT(*) FROM "+t).fetchone()[0] for t in ("defects","observations","game_performance_runs")}
record["db_counts"]=counts
EVID.write_text(json.dumps(record,indent=2)+"\n",encoding="utf-8")
DATA.write_text(json.dumps({"schema":"aios.underdog.veloren.runtime_defects.v1",
     "case_ids":[record["id"]],"cases":[{"id":record["id"],
      "status":record["status"],"hypotheses":record["hypotheses"],
      "reproduction_plan":record["reproduction_plan"],
      "evidence_path":str(EVID),"confidence":"user_reported"}]},indent=2)+"\n",encoding="utf-8")
HANDOFF.write_text("# Veloren terrain void + low FPS, Host 02: open investigation\n\n"
    "User reported severe lag and a temporary black hole of blocks beneath the character during actual play. "
    "**The hole was not independently captured, and its cause is unknown.**\n\n"
    f"Separate native D3D12 PresentMon forest witness: {perf['target_rows']} frames, "
    f"{perf['aggregate_present_fps']:.2f} average presented FPS, "
    f"{perf['gpu_time']['p50_ms']:.2f} ms GPU frame-time p50. "
    "This is not a measurement of the exact moment the hole occurred.\n\n"
    "Observed saved settings: 1920x1080, scale 1.0, clouds High, reflections High, mapped shadows, "
    "bloom On, fluid Medium, particles enabled, terrain distance 10, sprite distance 100.\n\n"
    "Likelihood of a graphics-cost bottleneck is supported by native performance data, "
    "but no proof yet that performance caused missing terrain geometry. "
    "Veloren official performance guide advises lowering rendering resolution and effects. "
    "Next controlled test: same saved singleplayer world; instrument chunk arrival, mesh upload and optional renderer validation; "
    "repeat native Dx12, then dx11 with equal graphics settings. Capture a frame when the hole appears. "
    "Avoid modifying the original settings or game executable.\n\n"
    f"Case evidence: {EVID}\nFull reproduction gates: {DATA}\n",encoding="utf-8")
lowfx_path=ROOT/"06_EVIDENCE"/"VELOREN_NATIVE_DX12_LOWFX065_V01.json"
if lowfx_path.is_file():
    lowfx=json.loads(lowfx_path.read_text(encoding="utf-8"))
    with HANDOFF.open("a",encoding="utf-8") as out:
        out.write("\n## Follow-up native effects control\n"
                  "Isolated scale-only trial: 208 presents, 6.11 FPS; "
                  "low-effects native trial: 423 presents, 12.24 FPS, GPU p50 83.80 ms. "
                  "Cloud Low, shadows None, bloom Off, point glow 0.0, fluid remains Medium. "
                  "Weather/camera not frame-matched; this is a separate exploratory performance witness, "
                  "not proof the terrain void is repaired or Underdog improved the game.\n"
                  "Receipt: "+str(lowfx_path)+"\n")
event={"id":record["id"],"utc":UTC,"type":"user_reported_gameplay_symptom",
       "claim":record["user_description"],"status":record["status"],
       "confidence":"USER_REPORT_ONLY","evidence":str(EVID),"cause":"UNKNOWN"}
lines=[json.loads(t) for t in LEDGER.read_text(encoding="utf-8").splitlines() if t.strip()]
if not any(x.get("id")==record["id"] for x in lines):
    with LEDGER.open("a",encoding="utf-8") as f:f.write(json.dumps(event)+"\n")
print(json.dumps({"status":"RECORDED_OPEN_UNREPRODUCED","counts":counts,
                  "evidence":str(EVID),"handoff":str(HANDOFF)},indent=2))
