#!/usr/bin/env python3
"""Record pinned wgpu API trace disablement as source-verified fact, not GPU failure."""
import hashlib,json,pathlib,sqlite3,subprocess
from datetime import datetime,timezone
R=pathlib.Path(__file__).resolve().parents[4]
S=R/"07_EXECUTION"/"VELOREN_TERRAIN_BUILD_EVAL_V01"/"source_sparse"
M=S/"voxygen"/"src"/"render"/"renderer"/"mod.rs"
T=S/"voxygen"/"Cargo.toml"
W=pathlib.Path(r"C:\Users\Dalton\.cargo\registry\src\index.crates.io-1949cf8c6b5b557f\wgpu-types-27.0.1\src\lib.rs")
DB=R/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
E=R/"06_EVIDENCE"/"VELOREN_WGPU_TRACE_DISABLED_SOURCE_V01.json"
K=R/"04_KNOWLEDGE"/"VELOREN_WGPU_TRACE_DISABLED_V01.md"
L=R/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for p in (M,T,W,DB,L):
    if not p.is_file():raise FileNotFoundError(p)
revision=subprocess.check_output(["git","-C",str(S),"rev-parse","HEAD"],text=True).strip()
assert revision=="da6626e6ed3c09619ed3e46423e6609dffadad22"
assert subprocess.run(["git","-C",str(S),"diff","--quiet","--","voxygen/src/render/renderer/mod.rs"]).returncode==0
m=M.read_text(encoding="utf-8").splitlines()
t=T.read_text(encoding="utf-8").splitlines()
w=W.read_text(encoding="utf-8").splitlines()
assert "#[cfg(any())]" in m[297] and "tracing" in m[297]
assert "trace: wgpu::Trace::Off," in m[343]
assert "WGPU_TRACE_DIR" in "\n".join(m[298:320])
assert any('wgpu = { version = "27", features = ["spirv", "glsl"] }'==x.strip() for x in t)
assert "Tracing is currently unavailable." in w[1500]
assert '#[cfg(feature = "trace")]' in w[1512]
assert "Directory(std::path::PathBuf)," in w[1515]
event={
 "schema":"aios.underdog.veloren.pinned_wgpu_trace_source.v1",
 "captured_utc":datetime.now(timezone.utc).isoformat(),
 "revision":revision,
 "renderer_path":str(M),"renderer_sha256":sha(M),
 "cargo_manifest_path":str(T),"cargo_manifest_sha256":sha(T),
 "wgpu_types_path":str(W),"wgpu_types_sha256":sha(W),
 "wgpu_version":"27","wgpu_types_version":"27.0.1",
 "proof":[
   {"file":"voxygen/src/render/renderer/mod.rs","line":298,"fact":"#[cfg(any())] excludes WGPU_TRACE_DIR setup"},
   {"file":"voxygen/src/render/renderer/mod.rs","line":344,"fact":"request_device trace field is unconditionally wgpu::Trace::Off"},
   {"file":"voxygen/Cargo.toml","fact":"wgpu features spirv, glsl; trace not requested"},
   {"file":"wgpu-types-27.0.1/src/lib.rs","lines":"1501-1516","fact":"Trace::Directory gated behind feature=trace; docs mark tracing currently unavailable"}
 ],
 "classification":"SOURCE_VERIFIED_API_TRACE_DISABLED_FOR_PINNED_GAME",
 "native_control":"WGPU_TRACE_DIR yielded zero files after native Dx12 world and movement",
 "interpretation":"No API trace files are expected from this code path; GPU display and rendering were independently observed",
 "not_claimed":"All versions of wgpu lack tracing; GPU did not submit commands; terrain void was solved",
 "separate_path":"Opt-in terrain event logging patch is independent of wgpu API tracing"
}
with sqlite3.connect(DB) as c:
    c.execute("PRAGMA foreign_keys=ON")
    assert c.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    c.execute("""CREATE TABLE IF NOT EXISTS veloren_trace_source_facts (
       fact_id TEXT PRIMARY KEY, revision TEXT NOT NULL, file_sha256 TEXT NOT NULL,
       source_range TEXT NOT NULL, finding TEXT NOT NULL, evidence_level TEXT NOT NULL,
       evidence_path TEXT NOT NULL, admitted_utc TEXT NOT NULL)""")
    factid="VELOREN-DA6626E6-WGPU-API-TRACE-OFF"
    old=c.execute("SELECT file_sha256 FROM veloren_trace_source_facts WHERE fact_id=?",(factid,)).fetchone()
    if old and old[0]!=event["renderer_sha256"]:raise RuntimeError("Immutable finding drift")
    c.execute("INSERT OR IGNORE INTO veloren_trace_source_facts VALUES(?,?,?,?,?,?,?,?)",
        (factid,revision,event["renderer_sha256"],"298-344",
         "WGPU_TRACE_DIR is cfg(any()) excluded; DeviceDescriptor trace Off",
         "PINNED_SOURCE_VERIFIED",str(E),event["captured_utc"]))
    assert c.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    c.commit()
E.write_text(json.dumps(event,indent=2)+"\n",encoding="utf-8")
K.write_text("# Why WGPU_TRACE_DIR produced no files on pinned Veloren\n\n"
 "Source verified at exact commit da6626e6ed3c09619ed3e46423e6609dffadad22. "
 "voxygen/src/render/renderer/mod.rs:298 has #[cfg(any())], excluding the environment-variable setup. "
 "Line 344 passes wgpu::Trace::Off unconditionally. voxygen/Cargo.toml requests wgpu27 spirv and glsl but not trace. "
 "Installed wgpu-types27.0.1 marks Trace::Directory behind feature trace and labels tracing unavailable. "
 "Accordingly, a zero-file WGPU_TRACE_DIR native run is expected for this pinned code and is not a D3D12 driver failure.\n\n"
 "This does not validate wgpu API tracing in new versions. The terrain lifecycle log patch remains a separate GPL consumer instrumentation route and is not yet built.\n\n"
 f"Evidence: {E}\n",encoding="utf-8")
ev={"id":"VELOREN-DA6626E6-WGPU-API-TRACE-OFF","utc":event["captured_utc"],
 "kind":"source_verified_api_trace_disabled","evidence":str(E),
 "confidence":"SOURCE_VERIFIED","effect":"explains native zero-file WGPU_TRACE_DIR"}
existing=[json.loads(line) for line in L.read_text(encoding="utf-8").splitlines() if line.strip()]
if not any(x.get("id")==ev["id"] for x in existing):
    with L.open("a",encoding="utf-8") as f:f.write(json.dumps(ev)+"\n")
print(json.dumps({"status":"WGPU_API_TRACE_SOURCE_VERIFIED","revision":revision,
 "evidence":str(E),"renderer_sha256":event["renderer_sha256"]}))
