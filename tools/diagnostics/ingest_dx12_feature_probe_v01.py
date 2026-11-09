#!/usr/bin/env python3
"""Idempotent native HD520 D3D12 capability and compute receipt ingestion."""
import hashlib, json, pathlib, re, sqlite3
from datetime import datetime, timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
DIR=ROOT/"07_EXECUTION"/"DX12_FEATURE_PROBE_V01"
RAW=DIR/"raw_capabilities.json"
ALU=DIR/"alu_stream_stdout.txt"
DATA=ROOT/"03_DATASET"/"DX12_FEATURE_MATRIX_V01.json"
DB=ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
SOURCE=ROOT/"09_REPOSITORY"/"underdog-d3d-compat"/"tools"/"diagnostics"/"dx12_feature_probe_v01.cpp"
REC=ROOT/"06_EVIDENCE"/"DX12_HD520_FEATURE_PROBE_V01.json"
LEDGER=ROOT/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
NOTE=ROOT/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_FINDINGS_V01.md"
STAMP=datetime.now(timezone.utc).isoformat()
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for p in (RAW,ALU,DATA,DB,SOURCE):
    if not p.is_file():raise FileNotFoundError(p)
obs=json.loads(RAW.read_text(encoding="utf-8-sig"))
alu=ALU.read_text(encoding="utf-16" if ALU.read_bytes().startswith(bytes([255,254])) else "utf-8-sig")
matrix=json.loads(DATA.read_text(encoding="utf-8"))
assert obs["schema"]=="underdog.dx12.feature_probe.v1"
assert obs["vendor_id"]==32902 and obs["device_id"]==6422
for key in ("feature_level","options","options1","options5","root_signature"):
    assert obs[key]["hr"]=="0x00000000",key
assert obs["feature_level"]["max"]==0xC100
assert obs["options"]["binding_tier"]==3 and obs["options"]["heap_tier"]==2 and obs["options"]["tiled_tier"]==3
assert next(r for r in obs["shader_models"] if r["request"]==101)["reported"]==97
assert next(r for r in obs["shader_models"] if r["request"]==102)["hr"]=="0x80070057"
assert len(obs["formats"])==8 and all(f["hr"]=="0x00000000" for f in obs["formats"])
assert "GPU_VENDOR=0x8086 GPU_DEVICE=0x1916" in alu
assert "FRAMES_COMPLETED=36 SAMPLES_CPU_VERIFIED=288" in alu
assert "DEVICE_REMOVED_REASON=0x00000000" in alu and "END_ALU_STREAM_TEST" in alu
tiers=re.findall(r"TIER=(\d+) FRAMES=(\d+) GPU_UPLOAD_P50_MS=([\d.]+).*?GPU_ALU_P50_MS=([\d.]+).*?GPU_ALU_P95_MS=([\d.]+)",alu)
assert len(tiers)==3 and [int(x[0]) for x in tiers]==[4,32,128]
marks={
 "feature_level":("advertised","12_1"),"shader_model":("advertised","6.1"),
 "root_signature":("advertised","1.1"),"resource_binding":("advertised",3),
 "resource_heap":("advertised",2),"tiled_resources":("advertised",3),
 "texture_formats":("advertised","8 successful API queries; execution unverified"),
 "wave_ops":("advertised",True),"conservative_raster":("advertised",3),
 "typed_uav":("advertised",True),"rov":("advertised",True),
 "dxr":("unsupported","tier0"),
 "compute":("executed_pass","SM5.1, 36 frames/288 CPU verified")}
for row in matrix["tests"]:
    if row["id"] in marks:
        row["current_status"],row["advertised"]=marks[row["id"]]
        row["source"]=str(ALU if row["id"]=="compute" else RAW)
        if row["id"]=="compute":row["execution"]="SM5.1 36-frame checksum pass"
fingerprint={"hardware_id":"host02-intel-hd-520","driver":"22.20.16.4836",
 "feature_level":"12_1","shader_model_reported":"6.1","shader_model_6_6_query":"E_INVALIDARG",
 "binding_tier":3,"resource_heap_tier":2,"tiled_tier":3,
 "root_signature":"1.1","wave_ops_advertised":obs["options1"]["wave_ops"],
 "conservative_raster_tier":obs["options"]["conservative_raster_tier"],
 "typed_uav_additional_reported":obs["options"]["typed_uav_additional"],
 "rov_reported":obs["options"]["rov"],
 "dxr_tier":obs["options5"]["raytracing_tier"],
 "formats":obs["formats"],"compute_sm51_gpu_frames":36,"compute_sm51_cpu_verified_samples":288,
 "compute_sm51_alu_gpu_p50_ms":{x[0]:float(x[3]) for x in tiers},
 "sm61_execution":"NOT_TESTED"}
receipt={"schema":"aios.underdog.dx12.feature_probe.v1","utc":STAMP,"fingerprint":fingerprint,
 "source_sha256":digest(SOURCE),"raw_sha256":digest(RAW),"compute_log_sha256":digest(ALU),
 "raw_path":str(RAW),"compute_log_path":str(ALU),"source_path":str(SOURCE),
 "qualification":"NATIVE_HARDWARE_QUERY_AND_SEPARATE_SM51_EXECUTION",
 "limitations":["No SM6.1 shader executed","Format support bits not execution tested",
                "No native-DXR hardware","No Underdog integration in Veloren"]}
# Preserve prior DB: additive tables only, atomic transaction and FK.
with sqlite3.connect(DB) as con:
    con.execute("PRAGMA foreign_keys=ON")
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    assert con.execute("SELECT gpu_driver FROM hardware_profiles WHERE hardware_id='host02-intel-hd-520'").fetchone()[0]=="22.20.16.4836"
    con.execute("CREATE TABLE IF NOT EXISTS dx12_feature_probe_runs(id TEXT PRIMARY KEY, hardware_id TEXT NOT NULL, driver TEXT NOT NULL, raw_sha TEXT NOT NULL, source_sha TEXT NOT NULL, evidence TEXT NOT NULL, captured_utc TEXT NOT NULL, FOREIGN KEY(hardware_id) REFERENCES hardware_profiles(hardware_id))")
    con.execute("CREATE TABLE IF NOT EXISTS dx12_feature_probe_facts(probe_id TEXT NOT NULL, feature_key TEXT NOT NULL, advertised TEXT, execution TEXT NOT NULL, query_hr TEXT, evidence TEXT NOT NULL, PRIMARY KEY(probe_id,feature_key), FOREIGN KEY(probe_id) REFERENCES dx12_feature_probe_runs(id))")
    pid="HD520-4836-CAPS-V01"
    old=con.execute("SELECT raw_sha FROM dx12_feature_probe_runs WHERE id=?",(pid,)).fetchone()
    if old and old[0]!=receipt["raw_sha256"]:raise RuntimeError("Immutable probe ID hash conflict")
    con.execute("INSERT OR IGNORE INTO dx12_feature_probe_runs VALUES(?,?,?,?,?,?,?)",(pid,"host02-intel-hd-520","22.20.16.4836",receipt["raw_sha256"],receipt["source_sha256"],str(REC),STAMP))
    facts=[("feature_level","12_1","advertised",obs["feature_level"]["hr"],RAW),
      ("shader_model","6.1","advertised","0x00000000",RAW),
      ("binding_tier","3","advertised",obs["options"]["hr"],RAW),
      ("heap_tier","2","advertised",obs["options"]["hr"],RAW),
      ("tiled_resources_tier","3","advertised",obs["options"]["hr"],RAW),
      ("root_signature","1.1","advertised",obs["root_signature"]["hr"],RAW),
      ("wave_ops",str(obs["options1"]["wave_ops"]),"advertised",obs["options1"]["hr"],RAW),
      ("conservative_raster_tier",str(obs["options"]["conservative_raster_tier"]),"advertised",obs["options"]["hr"],RAW),
      ("typed_uav_additional",str(obs["options"]["typed_uav_additional"]),"advertised",obs["options"]["hr"],RAW),
      ("rov",str(obs["options"]["rov"]),"advertised",obs["options"]["hr"],RAW),
      ("dxr","0","unsupported",obs["options5"]["hr"],RAW),
      ("compute_sm51","36 frames 288 samples","executed_pass","0x00000000",ALU)]
    facts += [(f["name"],"s1="+str(f["support1"])+" s2="+str(f["support2"]),"advertised",f["hr"],RAW) for f in obs["formats"]]
    for name,value,status,hr,path in facts:
        con.execute("INSERT OR REPLACE INTO dx12_feature_probe_facts VALUES(?,?,?,?,?,?)",(pid,name,value,status,hr,str(path)))
    con.execute("INSERT OR REPLACE INTO game_compatibility (game_id,hardware_id,title,status,native_baseline,underdog_integration,graphics_api,target_executable,evidence,blocking_reason,next_test,assessed_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
       ("veloren","host02-intel-hd-520","Veloren","in_progress","native_dx12_ingame_visual_pass","not_integrated","D3D12 via wgpu","C:\\Games\\Veloren\\BASELINE_2026-10-29\\game\\veloren-voxygen.exe",
        str(ROOT/"07_EXECUTION"/"VELOREN_NATIVE_BASELINE_V01"/"VELOREN_NATIVE_BASELINE_V01.json"),
        "Underdog A/B not performed","SM6.1 execution, resource format readback, opt-in wgpu audit",STAMP))
    con.execute("INSERT OR IGNORE INTO game_test_runs (run_id,game_id,hardware_id,started_utc,run_kind,launch_result,visual_result,gpu_status,fps_measurement,wrapper_version,evidence_path,notes) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
       ("VELOREN-NATIVE-DX12-V01","veloren","host02-intel-hd-520",STAMP,"native_unmodified","pass","ingame_hud_and_character","no_device_removed_observed",None,"none",str(ROOT/"07_EXECUTION"/"VELOREN_NATIVE_BASELINE_V01"/"VELOREN_NATIVE_BASELINE_V01.json"),"No measured FPS or verified travel displacement"))
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    con.commit()
    counts={t:con.execute("SELECT COUNT(*) FROM "+t).fetchone()[0] for t in ("dx12_feature_probe_runs","dx12_feature_probe_facts","game_compatibility","game_test_runs")}
receipt["database_counts"]=counts
matrix["latest_fingerprint"]=fingerprint
matrix["latest_receipt"]=str(REC)
tmp=DATA.with_name(DATA.name+".tmp")
tmp.write_text(json.dumps(matrix,indent=2)+"\n",encoding="utf-8");tmp.replace(DATA)
REC.write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
NOTE.write_text("# HD520 DX12 feature probe V01\n\n"
 +"Verified hardware API queries: FL12_1; Shader Model 6.1; binding Tier 3; heap Tier 2; tiled Tier 3; root signature 1.1; WaveOps true; DXR tier 0.\n\n"
 +"Eight FORMAT_SUPPORT queries succeeded: RGBA8, BGRA8, RGBA16F, BC1, BC3, BC7, D32F, RGBA8_UINT. Raw support bits preserved. Sampling/render/typed-UAV executions NOT tested.\n\n"
 +"Reused Underdog SM5.1 compute fixture on Intel HD 520, 36 frames, 288 CPU-verified values, 0 device removal. GPU compute median (ms) by tiers 4/32/128: "+str(fingerprint["compute_sm51_alu_gpu_p50_ms"])+". Not a Shader Model 6.1 execution test.\n\n"
 +"Source: "+str(SOURCE)+"\nRaw: "+str(RAW)+"\nCompute: "+str(ALU)+"\nReceipt: "+str(REC)+"\n\n"
 +"NEXT: execute SM6.1 wave intrinsic and compare readback; decode format flags, run BC sampling/typed UAV; measure paired real game presentation and memory; no driver changes or game patching.\n\n"
 +"Source and receipts retained; only regenerated build intermediates are cleanup candidates. Authority: Host02 execution, GitHub implementation, Drive governed canon only after review.\n",encoding="utf-8")
rec={"id":"HD520-4836-CAPS-V01","timestamp":STAMP,"claim":"HD520 reports SM6.1 and resource binding tier3; separate SM5.1 GPU compute 36 frames/288 verified samples",
     "qualification":"CAPABILITY_QUERIED_AND_SM51_EXECUTED_NOT_SM61","evidence":str(REC),
     "negative_control":"DXR tier0; SM6.6 query E_INVALIDARG; no WARP"}
prior=[json.loads(x) for x in LEDGER.read_text(encoding="utf-8").splitlines() if x.strip()] if LEDGER.exists() else []
if not any(x["id"]==rec["id"] for x in prior):
    with LEDGER.open("a",encoding="utf-8") as f:f.write(json.dumps(rec)+"\n")
print(json.dumps({"status":"INGEST_OK","counts":counts,"receipt":str(REC),"dataset":str(DATA),"knowledge":str(NOTE)},indent=2))
