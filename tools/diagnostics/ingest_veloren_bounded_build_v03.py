#!/usr/bin/env python3
"""Admit one immutable, bounded Veloren build-attempt witness. No fabricated runtime trace."""
import hashlib,json,pathlib,re,sqlite3,subprocess
from datetime import datetime,timezone
R=pathlib.Path(__file__).resolve().parents[4]
B=R/"07_EXECUTION"/"VELOREN_TERRAIN_BUILD_EVAL_V01"
S=B/"source_sparse"
ORIG=pathlib.Path(r"C:\Games\Veloren\BASELINE_2026-10-29\game")
RECEIPT=B/"BOUNDED_BUILD_V02.json"
LOG=B/"cargo_build_voxygen_v02_stderr.log"
PATCH=R/"07_EXECUTION"/"VELOREN_TERRAIN_TRACE_V01"/"veloren_da6626e6_terrain_trace_v01.patch"
TRACE=R/"06_EVIDENCE"/"VELOREN_WGPU_TRACE_DISABLED_SOURCE_V01.json"
E=R/"06_EVIDENCE"/"VELOREN_TERRAIN_BOUNDED_BUILD_V03.json"
D=R/"03_DATASET"/"VELOREN_TERRAIN_RUNTIME_GATE_V03.json"
K=R/"04_KNOWLEDGE"/"VELOREN_TERRAIN_RUNTIME_GATE_V03.md"
L=R/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
H=R/"08_HANDOFF"/"VELOREN_TERRAIN_TRACE_V03.json"
DB=R/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
UTC=datetime.now(timezone.utc).isoformat()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for f in (RECEIPT,LOG,PATCH,TRACE,DB,L,S/"Cargo.lock"):
    if not f.is_file():raise FileNotFoundError(f)
rec=json.loads(RECEIPT.read_text(encoding="utf-8-sig"))
assert rec["schema"]=="aios.underdog.veloren.bounded_rust_build.v2"
assert rec["stop_reason"]=="STOP_BOUNDED_TIME" and rec["instrumented_executable_exists"] is False
assert rec["source_modified_only_terrain"] is True
assert rec["target_bytes"] < rec["target_limit_bytes"]
assert rec["revision"]=="da6626e6ed3c09619ed3e46423e6609dffadad22"
assert not (B/"target"/"debug"/"veloren-voxygen.exe").is_file()
assert sha(ORIG/"veloren-voxygen.exe").upper()=="BBFF168F2B8B15FD7A4647DFDDA2204321E1C25C1A68A8AA3A465CFA21BDB730"
assert sha(ORIG/"userdata"/"voxygen"/"settings.ron").upper()=="845A6D4F90276EB79E4F58E6A8C572EDE8893E19C462664686D0A8BDD9DF3A76"
rev=subprocess.check_output(["git","-C",str(S),"rev-parse","HEAD"],text=True).strip()
assert rev==rec["revision"]
changes=subprocess.check_output(["git","-C",str(S),"status","--short","--untracked-files=no"],text=True).splitlines()
assert len(changes)==1 and changes[0].strip().endswith("voxygen/src/scene/terrain/mod.rs"),changes
assert not subprocess.run(["git","-C",str(S),"diff","--quiet","--","voxygen/src/render/renderer/mod.rs"]).returncode
raw=LOG.read_bytes().decode("utf-8-sig",errors="replace")
starts=len(re.findall(r"Compiling\s+[^\n]+",raw))
assert starts>=50,starts
assert "error:" not in raw.lower() and "error[" not in raw.lower()
assert "Finished " not in raw
assert not list((B/"target"/"debug").glob("veloren-voxygen.exe"))
trace=json.loads(TRACE.read_text(encoding="utf-8"))
assert trace["classification"]=="SOURCE_VERIFIED_API_TRACE_DISABLED_FOR_PINNED_GAME"
record={
 "schema":"aios.underdog.veloren.bounded_build_gate.v3",
 "run_id":"VELOREN-HD520-TERRAIN-BUILD-V03",
 "admitted_utc":UTC,
 "pinned_commit":rev,
 "host":"WINDOWS_EXECUTION_HOST_02",
 "hardware_id":"host02-intel-hd-520",
 "build_requested":"cargo build --locked -p veloren-voxygen --bin veloren-voxygen --no-default-features --features singleplayer -j 2",
 "build_outcome":"BOUNDED_STOP_DURING_DEPENDENCY_COMPILATION",
 "full_compilation_succeeded":False,
 "rust_source_trace_executed":False,
 "terrain_void_reproduced":False,
 "terrain_chunk_xy":None,
 "game_exe_original_unchanged":True,
 "original_settings_unchanged":True,
 "partial_compilation_crates_started":starts,
 "compiler_diagnostic_errors_observed":0,
 "target_cache_bytes_at_stop":rec["target_bytes"],
 "target_limit_bytes":rec["target_limit_bytes"],
 "stop_reason":rec["stop_reason"],
 "source_worktree":str(S),
 "source_changes":changes,
 "cargo_lock_sha256":sha(S/"Cargo.lock"),
 "patch_sha256":sha(PATCH),
 "build_log_path":str(LOG),
 "build_log_sha256":sha(LOG),
 "build_guard_receipt_path":str(RECEIPT),
 "build_guard_receipt_sha256":sha(RECEIPT),
 "native_wgpu_trace_source_status":"SOURCE_DISABLED_BY_CFG_ANY_AND_TRACE_OFF",
 "wgpu_evidence_path":str(TRACE),
 "next_gate":"Reuse retained dependency/build cache or move GPL consumer build to a separately licensed Windows CI project, then perform in-world moving trace and capture actual missing terrain coordinates.",
 "limits":["No compiled Veloren binary from this attempt","No new GPU fault evidence","No Underdog speedup","Build cache is recyclable only after a verified alternative build witness exists"]
}
with sqlite3.connect(DB) as c:
    c.execute("PRAGMA foreign_keys=ON")
    assert c.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    c.execute("""CREATE TABLE IF NOT EXISTS veloren_terrain_trace_gate_runs(
        run_id TEXT PRIMARY KEY,hardware_id TEXT NOT NULL,game_id TEXT NOT NULL,
        revision TEXT NOT NULL,patch_sha256 TEXT NOT NULL,cargo_log_sha256 TEXT NOT NULL,
        build_status TEXT NOT NULL,native_status TEXT NOT NULL,wgpu_files INTEGER NOT NULL,
        terrain_events INTEGER NOT NULL,evidence_path TEXT NOT NULL,utc TEXT NOT NULL,
        FOREIGN KEY(game_id,hardware_id) REFERENCES game_compatibility(game_id,hardware_id))""")
    old=c.execute("SELECT cargo_log_sha256 FROM veloren_terrain_trace_gate_runs WHERE run_id=?",
                  (record["run_id"],)).fetchone()
    if old and old[0]!=record["build_log_sha256"]:
        raise RuntimeError("Immutable Cargo build receipt conflict")
    c.execute("INSERT OR IGNORE INTO veloren_terrain_trace_gate_runs VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
      (record["run_id"],record["hardware_id"],"veloren",rev,record["patch_sha256"],
       record["build_log_sha256"],record["build_outcome"],"NOT_RUN_SOURCE_TRACE",0,0,str(E),UTC))
    assert c.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    c.commit()
    record["database_counts"]={
      t:c.execute("SELECT COUNT(*) FROM "+t).fetchone()[0]
      for t in ("veloren_terrain_trace_gate_runs","game_test_runs",
                "veloren_terrain_source_findings","veloren_trace_source_facts")}
E.write_text(json.dumps(record,indent=2)+"\n",encoding="utf-8")
D.write_text(json.dumps({"schema":"aios.underdog.veloren.terrain_runtime_dataset.v3",
     "pinned_commit":rev,"run_id":record["run_id"],"source_build":record["build_outcome"],
     "compiled_rust_crates_started":starts,"live_events":0,"game_patch_executed":False,
     "defect_status":"OPEN_UNREPRODUCED","source_wgpu_trace_disabled":True,
     "evidence":str(E)},indent=2)+"\n",encoding="utf-8")
K.write_text("# Veloren live terrain trace gate V03\n\n"
    "Bounded Host 02 native Windows Cargo build on exact revision da6626e6ed3c09619ed3e46423e6609dffadad22.\n"
    f"- {starts} Rust dependency compilation starts; no compiler diagnostic errors in captured log.\n"
    f"- Stopped at 420-second build guard, retaining approximately {rec['target_bytes']/1024/1024:.1f} MiB reusable target cache.\n"
    "- No compiled terrain-instrumented Voxygen executable. No lifecycle events, bad-frame chunk coordinates or demonstrated repair.\n"
    "- Official executable and original settings unchanged, separate source worktree only modifies terrain/mod.rs.\n"
    "- Native wgpu API trace was not emitted because pinned renderer source disables WGPU_TRACE_DIR and unconditionally passes wgpu::Trace::Off; this is independent of the planned terrain log.\n"
    "- Next route: bounded continuation with this cache or dedicated GPL-compliant Windows CI builder; do not vendor upstream GPL terrain source into MIT Underdog runtime.\n\n"
    f"Evidence: {E}\nCompiler log: {LOG}\n",encoding="utf-8")
prev=[json.loads(x) for x in L.read_text(encoding="utf-8").splitlines() if x.strip()]
event={"id":record["run_id"],"utc":UTC,"kind":"bounded_source_build_evaluation",
       "status":record["build_outcome"],"compiler_started_crates":starts,
       "retained_target_bytes":rec["target_bytes"],"runtime_events":0,
       "root_cause":"UNKNOWN","evidence":str(E)}
if not any(x.get("id")==record["run_id"] for x in prev):
    with L.open("a",encoding="utf-8") as f:f.write(json.dumps(event)+"\n")
H.write_text(json.dumps({"schema":"aios.underdog.veloren.handoff.v3",
    "captured_utc":UTC,"scope":"pc-game-re-modding / D3D_COMPAT_UNDERDOG_01",
    "canonical_authority":"Drive parent project; Host02 current execution and evidence only",
    "source_rev":rev,"build_status":record["build_outcome"],
    "trace_patch_path":str(PATCH),"source_checkout":str(S),
    "live_terrain_events":0,"reported_terrain_void":"OPEN_UNREPRODUCED",
    "source_wgpu_trace":"DISABLED_AT_PINNED_REVISION",
    "performance_comparisons":"NATIVE_EXPLORATORY_ONLY_NO_UNDERDOG_GAIN",
    "reusable_compile_cache":str(B/"target"),
    "cache_retention":"RETAINED_FOR_NEXT_BUILD_RESUME_UNTIL_VERIFIED_SUCCESSOR",
    "exact_evidence":str(E),"dataset":str(D),"database":str(DB),
    "knowledge":str(K),"next_gate":record["next_gate"]},indent=2)+"\n",encoding="utf-8")
print(json.dumps({"status":"BOUND_BUILD_CHECKPOINT_ADMITTED",
    "compiled_crates_started":starts,"target_cache_mb":round(rec["target_bytes"]/1048576,1),
    "db_counts":record["database_counts"],"evidence":str(E)},indent=2))
