#!/usr/bin/env python3
"""Admit bounded source-build checkpoint + native trace negative control; no invented live events."""
import hashlib, json, pathlib, re, sqlite3, subprocess
from datetime import datetime, timezone
R=pathlib.Path(__file__).resolve().parents[4]
B=R/"07_EXECUTION"/"VELOREN_TERRAIN_BUILD_EVAL_V01"
N=R/"07_EXECUTION"/"VELOREN_TERRAIN_NATIVE_WGPU_TRACE_V01"
P=R/"07_EXECUTION"/"VELOREN_TERRAIN_TRACE_V01"
DB=R/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
E=R/"06_EVIDENCE"/"VELOREN_TERRAIN_RUNTIME_GATE_V02.json"
D=R/"03_DATASET"/"VELOREN_TERRAIN_RUNTIME_GATE_V02.json"
K=R/"04_KNOWLEDGE"/"VELOREN_TERRAIN_RUNTIME_GATE_V02.md"
L=R/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
H=R/"08_HANDOFF"/"VELOREN_TERRAIN_TRACE_V02.json"
S=B/"source_sparse"
PATCH=P/"veloren_da6626e6_terrain_trace_v01.patch"
CARGO=B/"cargo_build_voxygen_v01.log"
STDOUT=N/"game_stdout.log"
GAME=pathlib.Path(r"C:\Games\Veloren\BASELINE_2026-10-29\game")
STAMP=datetime.now(timezone.utc).isoformat()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):
    data=p.read_bytes()
    return data.decode("utf-16" if data[:2] in (b"\xff\xfe",b"\xfe\xff") else "utf-8-sig",errors="replace")
needed=[PATCH,CARGO,STDOUT,DB,L,S/"rust-toolchain",S/"Cargo.lock",B/"cargo_metadata.json",
        N/"native_gpu_trace_ingame.png",N/"native_gpu_trace_post_w_9s.png",N/"native_gpu_trace_strfwd.png"]
for f in needed:
    if not f.is_file():raise FileNotFoundError(f)
assert read(S/"rust-toolchain").strip()=="nightly-2026-06-13"
rev=subprocess.check_output(["git","-C",str(S),"rev-parse","HEAD"],text=True).strip()
assert rev=="da6626e6ed3c09619ed3e46423e6609dffadad22"
changes=subprocess.check_output(["git","-C",str(S),"status","--porcelain","--untracked-files=no"],text=True).splitlines()
assert len(changes)==1 and changes[0].strip().endswith("voxygen/src/scene/terrain/mod.rs"),changes
patchrec=json.loads((P/"terrain_trace_patch_receipt_v01.json").read_text(encoding="utf-8"))
assert patchrec["patch_sha256"]==sha(PATCH)
assert len(json.loads(read(B/"cargo_metadata.json"))["packages"])==25
assert sha(GAME/"veloren-voxygen.exe").upper()=="BBFF168F2B8B15FD7A4647DFDDA2204321E1C25C1A68A8AA3A465CFA21BDB730"
assert sha(GAME/"userdata"/"voxygen"/"settings.ron").upper()=="845A6D4F90276EB79E4F58E6A8C572EDE8893E19C462664686D0A8BDD9DF3A76"
assert not (B/"target"/"debug"/"veloren-voxygen.exe").exists()
log=read(STDOUT)
assert "selected graphics device" in log and "Intel(R) HD Graphics 520" in log and "Dx12" in log
assert "underdog_terrain_trace" not in log
trace_files=[x for x in (N/"wgpu_trace").rglob("*") if x.is_file()]
assert not trace_files,"WGPU_TRACE_DIR not empty; reclassify rather than ignoring"
compile_starts=len(re.findall(r"Compiling ",read(CARGO)))
assert compile_starts>0 and "Finished" not in read(CARGO)
screens=[N/n for n in ("native_gpu_trace_menu.png","native_gpu_trace_ingame.png",
          "native_gpu_trace_post_w_9s.png","native_gpu_trace_post_w_20s.png",
          "native_gpu_trace_strfwd.png")]
assert all(x.is_file() for x in screens)
record={
 "schema":"aios.underdog.veloren.terrain_live_gate.v2",
 "utc":STAMP,"game_revision":rev,"hardware_id":"host02-intel-hd-520",
 "source_checkout":str(S),"nightly_toolchain":"2026-06-13",
 "cargo_lock_sha256":sha(S/"Cargo.lock"),"patch_sha256":sha(PATCH),
 "source_patch_status":"APPLIED_NOT_COMPILED","build_status":"BOUNDED_STOP_DURING_DEPS",
 "compile_start_log_lines":compile_starts,"build_log_path":str(CARGO),
 "build_log_sha256":sha(CARGO),"instrumented_executable_exists":False,
 "native_executable":"UNCHANGED_OFFICIAL","native_backend":"Intel HD Graphics 520 Dx12",
 "native_world":"LOADED_SINGLEPLAYER","movement_issued_ms":[9000,11000,4000,9000],
 "movement_gate":"GAME_RESPONSIVE_VISUALS_CHANGED_EXACT_DISPLACEMENT_UNMEASURED",
 "native_stdout_path":str(STDOUT),"native_stdout_sha256":sha(STDOUT),
 "native_screenshots":[{"path":str(f),"sha256":sha(f)} for f in screens],
 "wgpu_trace_dir":str(N/"wgpu_trace"),"wgpu_trace_files":0,
 "wgpu_trace_qualification":"NO_OUTPUT_FROM_RETAIL_BUILD_NOT_PROOF_GPU_COMMANDS_ABSENT",
 "source_terrain_events":0,"live_terrain_trace":"NOT_RUN",
 "black_hole_defect":"USER_REPORTED_NOT_REPRODUCED_IN_CAPTURE",
 "missing_chunk_coordinates":None,"root_cause":"UNKNOWN",
 "underdog_patch_in_game":False,"underdog_fps_gain":False,
 "original_executable_sha256":sha(GAME/"veloren-voxygen.exe"),
 "original_settings_sha256":sha(GAME/"userdata"/"voxygen"/"settings.ron"),
 "next_gate":"finish exact source build and run opt-in terrain events during movement; correlate missing chunk coordinates"
}
with sqlite3.connect(DB) as c:
    c.execute("PRAGMA foreign_keys=ON")
    assert c.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    assert c.execute("SELECT COUNT(*) FROM hardware_profiles WHERE hardware_id='host02-intel-hd-520'").fetchone()[0]==1
    c.execute("""CREATE TABLE IF NOT EXISTS veloren_terrain_trace_gate_runs(
        run_id TEXT PRIMARY KEY,hardware_id TEXT NOT NULL,game_id TEXT NOT NULL,
        revision TEXT NOT NULL,patch_sha256 TEXT NOT NULL,cargo_log_sha256 TEXT NOT NULL,
        build_status TEXT NOT NULL,native_status TEXT NOT NULL,wgpu_files INTEGER NOT NULL,
        terrain_events INTEGER NOT NULL,evidence_path TEXT NOT NULL,utc TEXT NOT NULL,
        FOREIGN KEY(game_id,hardware_id) REFERENCES game_compatibility(game_id,hardware_id))""")
    runid="VELOREN-HD520-TERRAIN-GATE-V02"
    old=c.execute("SELECT cargo_log_sha256 FROM veloren_terrain_trace_gate_runs WHERE run_id=?",(runid,)).fetchone()
    if old and old[0]!=record["build_log_sha256"]:raise RuntimeError("Immutable run ID/source log conflict")
    c.execute("INSERT OR IGNORE INTO veloren_terrain_trace_gate_runs VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
      (runid,"host02-intel-hd-520","veloren",rev,sha(PATCH),sha(CARGO),
       record["build_status"],"NATIVE_DX12_CONTROL_RAN",0,0,str(E),STAMP))
    c.execute("""INSERT OR IGNORE INTO game_test_runs
        (run_id,game_id,hardware_id,started_utc,run_kind,launch_result,visual_result,
         gpu_status,fps_measurement,wrapper_version,evidence_path,notes)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?)""",
      ("VELOREN-NATIVE-WGPU-TRACE-NEGATIVE-V01","veloren","host02-intel-hd-520",STAMP,
       "native_wgpu_trace_requested","successful","world_and_movement_no_hole_captured",
       "Intel_HD520_Dx12_device_no_removal_logged",None,"none",str(E),
       "WGPU_TRACE_DIR 0 files; not the source terrain trace"))
    assert c.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    c.commit()
    record["database_counts"]={
      t:c.execute("SELECT COUNT(*) FROM "+t).fetchone()[0]
      for t in ("veloren_terrain_trace_gate_runs","game_test_runs","veloren_terrain_source_findings")}
E.write_text(json.dumps(record,indent=2)+"\n",encoding="utf-8")
D.write_text(json.dumps({"schema":"aios.underdog.veloren.terrain_gate_dataset.v2",
   "run_id":runid,"evidence":str(E),"source_built":False,"live_events":0,
   "native_dx12_world":"verified","wgpu_trace":"NO_OUTPUT",
   "defect":"OPEN_UNREPRODUCED","next_gate":record["next_gate"]},indent=2)+"\n",encoding="utf-8")
K.write_text("# Veloren Host02 terrain runtime gate V02\n\n"
 f"Pinned sparse source {rev}, Rust nightly 2026-06-13, native MSVC/CMake/Ninja. "
 f"Cargo metadata accepted 25 packages. Build deliberately stopped in dependency compilation after {compile_starts} compile-start lines; no instrumented executable was produced.\n\n"
 "Separately launched the original binary on Intel HD 520 Dx12 with WGPU_TRACE_DIR and an isolated copied single-player profile. "
 "Reached the saved world, sent bounded movement inputs, captured full-resolution screenshots. The game exited gracefully. "
 "No captured black-hole frame and no measured terrain chunk coordinates. "
 "WGPU_TRACE_DIR produced zero files; an empty trace does not prove the GPU issued no commands and may indicate a missing build feature.\n\n"
 "Only source mapping and patch syntax have passed for UNDERDOG_TERRAIN_TRACE; no live lifecycle events exist. "
 "Root cause remains unknown. Do not claim Underdog improved FPS or fixed terrain.\n\n"
 f"Receipt: {E}\nBuild log: {CARGO}\nNative stdout: {STDOUT}\n",encoding="utf-8")
prev=[json.loads(l) for l in L.read_text(encoding="utf-8").splitlines() if l.strip()]
ev={"id":runid,"utc":STAMP,"kind":"runtime_gate_evaluation","status":"BUILD_INCOMPLETE_NATIVE_WGPU_TRACE_NO_OUTPUT",
    "evidence":str(E),"terrain_void":"NOT_CAPTURED","root_cause":"UNKNOWN"}
if not any(x.get("id")==runid for x in prev):
    with L.open("a",encoding="utf-8") as f:f.write(json.dumps(ev)+"\n")
H.write_text(json.dumps({"schema":"aios.underdog.veloren.terrain_handoff.v2",
    "utc":STAMP,"scope":"pc-game-re-modding / D3D_COMPAT_UNDERDOG_01",
    "source_revision":rev,"build_status":record["build_status"],
    "native_status":"INTEL_DX12_WORLD_AND_MOVEMENT_VERIFIED",
    "wgpu_trace":"NO_OUTPUT","terrain_lifecycle_trace":"NOT_EXECUTED",
    "reported_void":"OPEN_UNREPRODUCED","evidence":str(E),
    "dataset":str(D),"database":str(DB),"knowledge":str(K),
    "next_gate":record["next_gate"],"retained_source_worktree":str(S),
    "original_game_unchanged":True},indent=2)+"\n",encoding="utf-8")
print(json.dumps({"status":"ADMITTED","compile_start_lines":compile_starts,
 "native_control":"PASS","wgpu_trace_file_count":0,"live_source_trace":False,
 "db_counts":record["database_counts"],"evidence":str(E)},indent=2))
