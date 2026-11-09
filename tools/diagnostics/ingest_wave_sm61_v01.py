#!/usr/bin/env python3
"""Admit only a verified native Intel GPU DXIL SM6.1 WaveOps execution witness."""
import hashlib,json,pathlib,sqlite3
from datetime import datetime,timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
RUN=ROOT/"07_EXECUTION"/"DX12_FEATURE_PROBE_V01"
LOG=RUN/"wave_sm61_stdout.txt"
CODE=ROOT/"09_REPOSITORY"/"underdog-d3d-compat"/"tools"/"diagnostics"/"wave_sm61_dispatch_v01.cpp"
SHADER=CODE.with_name("wave_sm61_v01.hlsl")
CSO=RUN/"wave_sm61_v01.cso"
DATA=ROOT/"03_DATASET"/"DX12_FEATURE_MATRIX_V01.json"
DB=ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
OUT=ROOT/"06_EVIDENCE"/"DX12_HD520_SM61_WAVE_V01.json"
LEDGER=ROOT/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
NOTE=ROOT/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_FINDINGS_V01.md"
for f in (LOG,CODE,SHADER,CSO,DATA,DB,NOTE):
    if not f.is_file():raise FileNotFoundError(f)
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
log=LOG.read_text(encoding="utf-16" if LOG.read_bytes().startswith(bytes([255,254])) else "utf-8-sig")
for token in ("SM61_CREATE_COMPUTE_PSO_HR=0x00000000",
              "GPU_VENDOR=0x8086 GPU_DEVICE=0x1916",
              "SM61_WAVE_RESULTS=32/32 MIN_LANES=16 MAX_LANES=16",
              "DEVICE_REMOVED_REASON=0x00000000",
              "SM61_WAVE_EXECUTION_PASS"):
    if token not in log:raise AssertionError("Wave witness missing: "+token)
matrix=json.loads(DATA.read_text(encoding="utf-8"))
for feature in matrix["tests"]:
    if feature["id"] in ("shader_model","wave_ops"):
        feature["current_status"]="executed_pass"
        feature["execution"]="SM6.1 bounded WaveGetLaneCount+WaveActiveSum dispatch; 32/32 readback; 16-lane wave"
        feature["source"]=str(OUT)
stamp=datetime.now(timezone.utc).isoformat()
receipt={"schema":"aios.underdog.sm61.wave_execution.v1","captured_utc":stamp,
         "hardware_id":"host02-intel-hd-520","driver":"22.20.16.4836",
         "shader_model":"6.1","gpu_wave_size_observed":16,"verified_values":32,
         "pso_hr":"0x00000000","device_removed_reason":"0x00000000",
         "log_sha256":digest(LOG),"source_sha256":digest(CODE),
         "hlsl_sha256":digest(SHADER),"compiled_shader_sha256":digest(CSO),
         "log_path":str(LOG),"source_path":str(CODE),"shader_source_path":str(SHADER),
         "qualification":"EXECUTED_SM61_ONE_SHADER_ON_NATIVE_INTEL_HARDWARE",
         "limits":"Single 32-thread dispatch. No claim about all SM6.1 operations, other wave widths, or game performance."}
pid="HD520-4836-SM61-WAVE-V01"
with sqlite3.connect(DB) as con:
    con.execute("PRAGMA foreign_keys=ON")
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    assert con.execute("SELECT COUNT(*) FROM dx12_feature_probe_runs WHERE id='HD520-4836-CAPS-V01'").fetchone()[0]==1
    prior=con.execute("SELECT raw_sha FROM dx12_feature_probe_runs WHERE id=?",(pid,)).fetchone()
    if prior and prior[0]!=receipt["log_sha256"]:raise RuntimeError("immutable id hash conflict")
    con.execute("INSERT OR IGNORE INTO dx12_feature_probe_runs VALUES(?,?,?,?,?,?,?)",
        (pid,"host02-intel-hd-520","22.20.16.4836",receipt["log_sha256"],receipt["source_sha256"],str(OUT),stamp))
    for key in ("shader_model_6_1_wave","wave_ops_sm61"):
        con.execute("INSERT OR REPLACE INTO dx12_feature_probe_facts VALUES(?,?,?,?,?,?)",
          (pid,key,"16-lane wave, 32/32 outputs","executed_pass","0x00000000",str(LOG)))
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    con.commit()
    counts={t:con.execute("SELECT COUNT(*) FROM "+t).fetchone()[0] for t in ("dx12_feature_probe_runs","dx12_feature_probe_facts")}
matrix["latest_wave_sm61_receipt"]=str(OUT)
matrix["sm61_execution"]="one_32_thread_dispatch_verified_32_of_32_16_lane_wave"
matrix["latest_fingerprint"]["sm61_execution"]="ONE_WAVE_SHADER_EXECUTED_32_OF_32"
tmp=DATA.with_name(DATA.name+".tmp")
tmp.write_text(json.dumps(matrix,indent=2)+"\n",encoding="utf-8");tmp.replace(DATA)
receipt["db_counts"]=counts
OUT.write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
add="\n## Native Shader Model 6.1 execution: new verified V01 witness\nCompiled HLSL cs_6_1 via DXC, created D3D12 compute PSO, executed WaveGetLaneCount + WaveActiveSum on Intel HD 520, read back 32/32 valid outputs. Wave size was 16 lanes. No device removal. This is one shader workload, not blanket SM6.1 coverage.\nReceipt: "+str(OUT)+"\n"
n=NOTE.read_text(encoding="utf-8")
if "## Native Shader Model 6.1 execution: new verified V01 witness" not in n:NOTE.write_text(n+add,encoding="utf-8")
existing=[json.loads(x) for x in LEDGER.read_text(encoding="utf-8").splitlines() if x.strip()]
event={"id":pid,"utc":stamp,"claim":"SM6.1 WaveOps dispatch on Intel HD 520: 32/32 output entries, 16 lanes, no device removal",
       "confidence":"native_gpu_readback","evidence":str(OUT),"limits":"only one bounded shader workload"}
if not any(x.get("id")==pid for x in existing):
    with LEDGER.open("a",encoding="utf-8") as f:f.write(json.dumps(event)+"\n")
print(json.dumps({"status":"INGEST_SM61_OK","counts":counts,"receipt":str(OUT)}))
