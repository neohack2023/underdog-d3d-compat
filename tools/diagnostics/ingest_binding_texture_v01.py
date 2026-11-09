#!/usr/bin/env python3
"""Idempotent hardware fixture result ingestion with immutable raw hash gates."""
import json,hashlib,pathlib,sqlite3
from datetime import datetime,timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
RUN=ROOT/"07_EXECUTION"/"DX12_BINDING_TEXTURE_V01"
EVID=ROOT/"06_EVIDENCE"/"DX12_HD520_BINDING_TEXTURE_V01.json"
DATA=ROOT/"03_DATASET"/"DX12_FEATURE_MATRIX_V01.json"
DB=ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
LEDGER=ROOT/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
KNOW=ROOT/"04_KNOWLEDGE"/"DX12_HD520_BINDING_TEXTURE_FINDINGS_V01.md"
checks={
"sample_v04":("BCN_SAMPLE_V04.txt",["FORMAT_SHADER_SAMPLE_PASS="+f for f in ("BC1","BC3","BC7","RGBA16F")]+["MIP_SHADER_SAMPLES_PASS=16","END_BCN_SHADER_SAMPLE_V04"]),
"mip_v02":("BC_MIP_V02.txt",["FORMAT_PASS="+f for f in ("BC1","BC3","RGBA8","BC7","RGBA16F")]+["MIPS_COPIED_AND_CHECKED=160 FRAMES_COMPLETED=40","END_BC_MIP_STREAM_V02"]),
"uav_float":("UAV_FLOAT.txt",["FORMAT_MODE=R32G32B32A32_FLOAT","MATRIX_PASS=1","FINAL_STRUCT_ROOT=1 FINAL_STRUCT_TABLE=1 FINAL_TYPED_ROOT_PSO=0 FINAL_TYPED_TABLE=1"]),
"uav_rgba8":("UAV_RGBA8.txt",["FORMAT_MODE=ORIGINAL_RGBA8_UNORM","RGBA8_BYTES=64,128,191,255","MATRIX_PASS=1"]),
"tier3":("TIER3_SPARSE_UAV.txt",["RESOURCE_BINDING_TIER=3","TIER3_PSO_HR=0x00000000","TIER3_LIVE_INDEX=64 OF 65 POPULATED_DESCRIPTOR_COUNT=1","TIER3_READBACK=0x13BEEF42 EXPECT=0x13BEEF42","TIER3_SPARSE_UAV_EXECUTION_PASS"])}
def hashfile(p):return hashlib.sha256(p.read_bytes()).hexdigest()
raw={}
for name,(filename,tokens) in checks.items():
    path=RUN/filename
    if not path.is_file():raise FileNotFoundError(path)
    text=path.read_text(encoding="utf-16" if path.read_bytes().startswith(bytes([255,254])) else "utf-8-sig")
    assert all(token in text for token in tokens),name
    raw[name]={"path":str(path),"sha256":hashfile(path),"witnesses":tokens}
assert all("DEVICE_REMOVED_REASON=0x00000000" in (RUN/checks[n][0]).read_text(encoding="utf-16") for n in ("sample_v04","mip_v02","tier3"))
matrix=json.loads(DATA.read_text(encoding="utf-8"))
for row in matrix["tests"]:
    if row["id"]=="resource_binding":row.update(current_status="executed_pass",execution="Tier3 sparse 65-UAV table: index64 populated and readback matched",source=str(EVID))
    if row["id"]=="texture_formats":row.update(current_status="executed_pass",execution="BC1/BC3/BC7/RGBA16F shader Load 16/16; five formats upload/readback 160/160",source=str(EVID))
    if row["id"]=="typed_uav":row.update(current_status="executed_pass",execution="FLOAT4 + RGBA8 structured root/table and typed table: passed; invalid typed root rejected",source=str(EVID))
matrix["latest_binding_texture_receipt"]=str(EVID)
now=datetime.now(timezone.utc).isoformat()
combo=hashlib.sha256("".join(raw[k]["sha256"] for k in sorted(raw)).encode()).hexdigest()
receipt={"schema":"aios.underdog.binding_texture.v1","utc":now,
 "hardware_id":"host02-intel-hd-520","driver":"22.20.16.4836",
 "witnesses":{"shader_load_mips":16,"exact_copy_subresources":160,"copy_frames":40,
              "tier3_descriptor_slots":65,"tier3_used_slot":64,"tier3_readback":"0x13BEEF42",
              "uav_valid_cases":6,"typed_root_negative_controls":2},
 "raw":raw,
 "limitations":["One Tier3 sparse descriptor-table execution, not exhaustive conformance",
                "No filter interpolation testing","RGBA16F sampled only exact half 0/1, not HDR blending",
                "No Underdog Veloren integration or measured frame gain"]}
pid="HD520-4836-BINDING-TEXTURE-V01"
with sqlite3.connect(DB) as con:
    con.execute("PRAGMA foreign_keys=ON")
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    assert con.execute("SELECT gpu_driver FROM hardware_profiles WHERE hardware_id='host02-intel-hd-520'").fetchone()[0]=="22.20.16.4836"
    prev=con.execute("SELECT raw_sha FROM dx12_feature_probe_runs WHERE id=?",(pid,)).fetchone()
    if prev and prev[0]!=combo:raise RuntimeError("Immutable run conflict")
    con.execute("INSERT OR IGNORE INTO dx12_feature_probe_runs VALUES(?,?,?,?,?,?,?)",
                (pid,"host02-intel-hd-520","22.20.16.4836",combo,"owned-underdog-fixtures-extended",str(EVID),now))
    facts=[
     ("bc1_bc3_bc7_rgba16f_sample","16/16 shader mips","executed_pass","0x00000000",raw["sample_v04"]["path"]),
     ("bc1_bc3_bc7_rgba8_rgba16f_copy","160/160 copied mip subresources","executed_pass","0x00000000",raw["mip_v02"]["path"]),
     ("typed_uav_float","root/table policy verified","executed_pass","0x00000000",raw["uav_float"]["path"]),
     ("typed_uav_rgba8","root/table policy verified","executed_pass","0x00000000",raw["uav_rgba8"]["path"]),
     ("typed_root_invalid","invalid typed-root PSO rejected","expected_rejection","0x80070057",raw["uav_rgba8"]["path"]),
     ("tier3_sparse_uav","65 declared UAV, one populated index 64, readback matched","executed_pass","0x00000000",raw["tier3"]["path"])]
    for key,value,status,hr,path in facts:
        con.execute("INSERT OR REPLACE INTO dx12_feature_probe_facts VALUES(?,?,?,?,?,?)",(pid,key,value,status,hr,path))
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    con.commit()
    counts={k:con.execute("SELECT COUNT(*) FROM "+k).fetchone()[0] for k in ("dx12_feature_probe_runs","dx12_feature_probe_facts")}
receipt["db_counts"]=counts
tmp=DATA.with_name(DATA.name+".tmp")
tmp.write_text(json.dumps(matrix,indent=2)+"\n",encoding="utf-8");tmp.replace(DATA)
EVID.write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
KNOW.write_text("# HD520 D3D12 binding and texture V01\n\nGPU: Intel HD Graphics 520, driver 22.20.16.4836, hardware, not WARP.\n\n"+
 "BC1, BC3, BC7 and RGBA16F shader Load passed 16/16 CPU-expected mip colors. BC7 mode6 returned 0xFE000000, RGBA16F used exact half 0/1. No proof of filtering or HDR blending.\n\n"+
 "BC1, BC3, BC7, RGBA8, RGBA16F upload and copy byte readback 160/160 across 40 bounded frames. No device removal.\n\n"+
 "Structured root/table and typed UAV table GPU validation passed in FLOAT4 and RGBA8. Invalid typed texture root PSO rejected with E_INVALIDARG.\n\n"+
 "Tier3-specific 65-UAV table with only descriptor 64 populated, dynamic shader selection, returned 0x13BEEF42. Single successful pattern, not full Tier3 test suite.\n\n"+
 "Evidence receipt: "+str(EVID)+"\nNext: Veloren source-isolated opt-in audit and 120-frame paired performance. Do not claim Underdog improvement yet.\n",encoding="utf-8")
records=[json.loads(x) for x in LEDGER.read_text(encoding="utf-8").splitlines() if x.strip()] if LEDGER.is_file() else []
if not any(x.get("id")==pid for x in records):
    with LEDGER.open("a",encoding="utf-8") as fp:
        fp.write(json.dumps({"id":pid,"utc":now,"claim":"native GPU Tier3 sparse UAV + BC1/BC3/BC7/RGBA16F sample/readback + typed UAV pass","qualification":"bounded hardware witness","evidence":str(EVID)})+"\n")
print(json.dumps({"status":"INGEST_OK","counts":counts,"receipt":str(EVID)}))
