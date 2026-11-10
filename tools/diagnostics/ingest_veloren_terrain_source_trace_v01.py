#!/usr/bin/env python3
"""Idempotent guarded source-finding admission. Never mislabels static proof as runtime proof."""
import hashlib,json,pathlib,sqlite3
from datetime import datetime,timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
WORK=ROOT/"07_EXECUTION"/"VELOREN_TERRAIN_TRACE_V01"
ORIGINAL=WORK/"upstream_terrain_da6626e6.rs"
PATCH=WORK/"veloren_da6626e6_terrain_trace_v01.patch"
PATCHREC=WORK/"terrain_trace_patch_receipt_v01.json"
NEG=WORK/"unchanged_native_log_trace_parse_v01.json"
DB=ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
DATA=ROOT/"03_DATASET"/"VELOREN_TERRAIN_LIFECYCLE_V01.json"
KNOW=ROOT/"04_KNOWLEDGE"/"VELOREN_TERRAIN_TRACE_FINDINGS_V01.md"
LEDGER=ROOT/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
EVID=ROOT/"06_EVIDENCE"/"VELOREN_TERRAIN_SOURCE_TRACE_V01.json"
STAMP=datetime.now(timezone.utc).isoformat()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for p in (ORIGINAL,PATCH,PATCHREC,NEG,DB,LEDGER):
    if not p.is_file():raise FileNotFoundError(p)
rec=json.loads(PATCHREC.read_text(encoding="utf-8"))
neg=json.loads(NEG.read_text(encoding="utf-8"))
assert rec["game_revision"]=="da6626e6"
assert rec["status"]=="SOURCE_INSTRUMENTATION_CREATED_NOT_COMPILED_NOT_EXECUTED"
assert rec["original_sha256"]==sha(ORIGINAL)
assert rec["patch_sha256"]==sha(PATCH)
assert rec["edit_count"]==14
assert neg["status"]=="NO_TERRAIN_TRACE_EVENTS_FOUND" and neg["total_parsed_events"]==0
assert "OLDER_RESULT_ADMITTED" in rec["notable_events"]
source=ORIGINAL.read_text(encoding="utf-8")
rules=[
 ("NEIGHBOR_GATE","required 3x3 terrain neighborhood before chunk queue",
  900,921,"SOURCE_VERIFIED_RUNTIME_UNTESTED","Missing a neighboring chunk can defer mesh queue admission"),
 ("SAMPLE_MISSING_RETRY","sampling a missing chunk continues without launching meshing worker",
  1075,1083,"SOURCE_VERIFIED_RUNTIME_UNTESTED","Pending mesh can remain without a completed worker when sampled neighborhood lacks data"),
 ("NEAREST_FIRST_WORKER_BUDGET","nearest-first scheduling and CPU-dependent bounded meshing workers",
  1028,1059,"SOURCE_VERIFIED_RUNTIME_UNTESTED","Backlog/slow workers could delay geometry during movement"),
 ("RECV_FRAME_BUDGET","receive mesh results at ~240 per second scaled by delta time",
  1136,1148,"SOURCE_VERIFIED_RUNTIME_UNTESTED","Response processing is explicitly metered, capture queue length and frame latency"),
 ("OLDER_TICK_ADMITTED","response.started_tick <= todo.started_tick accepted; old result does not clear todo",
  1149,1261,"SOURCE_VERIFIED_RUNTIME_UNTESTED","Older results may briefly be installed while a newer remesh remains queued; correctness effect unknown"),
 ("ATLAS_ALLOCATION_RESET","on atlas allocation failure create replacement atlas and clear allocation IDs",
  1177,1198,"SOURCE_VERIFIED_RUNTIME_UNTESTED","Investigate atlas reset events if missing pixels persist after mesh storage"),
 ("MODEL_CREATE_AND_ATLAS_WRITE","update col_lights and kinds atlas, create terrain model and store chunk",
  1206,1251,"SOURCE_VERIFIED_RUNTIME_UNTESTED","CPU-side insertion is not GPU-present proof"),
 ("FRUSTUM_VISIBILITY_ONLY","Visibility::is_visible tests in_frustum; distance flag currently ignored",
  63,72,"SOURCE_VERIFIED_RUNTIME_UNTESTED","A chunk can be withheld by frustum or be absent; in_range alone is not draw gate"),
 ("DRAW_GATE","terrain render requires chunk map entry, in_frustum and opaque_model",
  1605,1643,"SOURCE_VERIFIED_RUNTIME_UNTESTED","Model eligibility is not proof of DX12 actual draw or presentation"),
]
assert "if response.started_tick <= todo.started_tick" in source
assert "self.in_frustum" in source
assert "CHUNKS_PER_SECOND: f32 = 240.0" in source
assert "Some((\n                    rpos,\n                    chunk.opaque_model.as_ref()?" in source
items=[{"key":k,"finding":name,"source_line_start":start,"source_line_end":end,
        "evidence_level":state,"hypothesis_not_confirmed":hyp}
       for k,name,start,end,state,hyp in rules]
report={"schema":"aios.underdog.veloren.terrain_lifecycle_source_map.v1",
 "observed_utc":STAMP,"game_ref":"da6626e6",
 "upstream_uri":"https://gitlab.com/veloren/veloren/-/blob/da6626e6/voxygen/src/scene/terrain/mod.rs",
 "source_sha256":sha(ORIGINAL),
 "source_path":str(ORIGINAL),"source_license":"GPL-3.0, isolated from MIT repo",
 "trace_patch_path":str(PATCH),"trace_patch_sha256":sha(PATCH),
 "trace_patch_check":"GIT_APPLY_AND_REVERSE_PASS_RUSTFMT_PARSE_PASS",
 "trace_compiled":False,"trace_executed":False,
 "unmodified_game_events":0,"parser_self_test":"6_SYNTHETIC_PASS",
 "findings":items,
 "player_visual_hole":{"status":"USER_REPORTED_UNREPRODUCED",
  "cause":"UNKNOWN","terrain_chunk_xy":"UNKNOWN","matching_bad_frame":"NOT_CAPTURED"},
 "next_gate":"Compile pinned GPL consumer trace with Rust/Cargo, then execute opt-in log on same world during controlled moving route",
 "limits":["Source interpretation is not hardware execution proof",
           "No actual GPU present instrumentation has been added",
           "Frame cost may be draw, mesh, game logic or a mixture",
           "Existing compiled retail binary does not contain these trace events"]}
with sqlite3.connect(DB) as con:
    con.execute("PRAGMA foreign_keys=ON")
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    con.execute("""CREATE TABLE IF NOT EXISTS veloren_terrain_source_findings(
        id TEXT PRIMARY KEY, game_id TEXT NOT NULL, hardware_id TEXT NOT NULL,
        source_ref TEXT NOT NULL, source_sha TEXT NOT NULL, source_range TEXT NOT NULL,
        finding TEXT NOT NULL, interpretation TEXT NOT NULL,
        evidence_level TEXT NOT NULL, patch_sha TEXT NOT NULL,
        evidence_path TEXT NOT NULL, admitted_utc TEXT NOT NULL,
        FOREIGN KEY(game_id,hardware_id) REFERENCES game_compatibility(game_id,hardware_id))""")
    for item in items:
        uid="VLR-DA6626E6-"+item["key"]
        old=con.execute("SELECT source_sha,patch_sha FROM veloren_terrain_source_findings WHERE id=?",(uid,)).fetchone()
        if old and old!=(report["source_sha256"],report["trace_patch_sha256"]):
            raise RuntimeError("Immutable source-finding conflict: "+uid)
        con.execute("INSERT OR IGNORE INTO veloren_terrain_source_findings VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
            (uid,"veloren","host02-intel-hd-520","da6626e6",
             report["source_sha256"],str(item["source_line_start"])+"-"+str(item["source_line_end"]),
             item["finding"],item["hypothesis_not_confirmed"],item["evidence_level"],
             report["trace_patch_sha256"],str(EVID),STAMP))
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    con.commit()
    count=con.execute("SELECT COUNT(*) FROM veloren_terrain_source_findings").fetchone()[0]
report["db_static_findings"]=count
EVID.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
DATA.write_text(json.dumps({"schema":"aios.underdog.veloren.terrain_lifecycle_dataset.v1",
 "game_ref":"da6626e6","hardware_id":"host02-intel-hd-520",
 "trace_patch_sha256":report["trace_patch_sha256"],"event_status":"NOT_EXECUTED",
 "reported_visual_defect":"OPEN_UNREPRODUCED",
 "source_findings":items,"evidence_path":str(EVID)},indent=2)+"\n",encoding="utf-8")
KNOW.write_text("# Veloren terrain lifecycle trace V01, verified source map\n\n"
 "Game revision da6626e6, native Intel HD 520. GPL source isolated under 07_EXECUTION; MIT repo carries generic parser/documentation only.\n\n"
 "## Verified source-control flow (not reproduced defect)\n"
 + "\n".join(f"- **{x['key']}**, lines {x['source_line_start']}-{x['source_line_end']}: {x['finding']}. Investigation: {x['hypothesis_not_confirmed']}." for x in items)
 + "\n\nParticularly important: the old worker-response comparison accepts started_tick <= the current todo tick, and will only clear todo when ticks match. Whether this produces missing terrain is unknown; instrument before changing behavior.\n"
 "The visibility method currently uses only frustum visibility, not the in_range flag. Missing an opaque_model means render filter drops the chunk, though an empty/air chunk might legitimately have none.\n\n"
 "## Tool and validation status\n"
 f"Trace patch: {PATCH}\nPatch SHA256: {sha(PATCH)}\n"
 "Patch applies/reverses and passes rustfmt syntax parse. It does not change meshing or rendering state. "
 "It emits up to 8192 opt-in log events when UNDERDOG_TERRAIN_TRACE=1. Rust/Cargo available on Host02. Full Veloren build not completed. "
 "Previous unmodified logs have zero new trace events, as expected. "
 "Generic log analyzer self-test passes with six synthetic events, not hardware data.\n"
 "\n## Next gate\n"
 "Build pinned Veloren GPL source in isolated workspace; run identical singleplayer world with moving player and capture a bad-frame chunk coordinate. "
 "Follow NEIGHBOR_WAIT → QUEUED → WORKER_START → WORKER_RECV → ATLAS_RESET/UPLOAD_BEGIN → MODEL_STORED → FOCUS_DRAW_GATE. "
 "Do not infer GPU present from CPU model creation. If accepted draw but missing image, add render pass/GPU timestamp instrumentation separately.\n",encoding="utf-8")
event={"id":"VELOREN-DA6626E6-TERRAIN-SOURCE-TRACE-V01","utc":STAMP,
 "kind":"source_control_flow_map","status":"VERIFIED_SOURCE_NOT_RUNTIME",
 "source_sha256":report["source_sha256"],"patch_sha256":report["trace_patch_sha256"],
 "evidence":str(EVID),"unresolved":"Visual void not reproduced, full instrumentation not built"}
prior=[json.loads(line) for line in LEDGER.read_text(encoding="utf-8").splitlines() if line.strip()]
if not any(x.get("id")==event["id"] for x in prior):
    with LEDGER.open("a",encoding="utf-8") as f:f.write(json.dumps(event)+"\n")
print(json.dumps({"status":"SOURCE_TRACE_INGEST_OK",
 "facts":len(items),"db_rows":count,"trace_compiled":False,
 "evidence":str(EVID),"dataset":str(DATA)}))
