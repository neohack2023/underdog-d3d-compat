#!/usr/bin/env python3
"""Governed Host02 ingestion: one 8-row ABI semantic projection + three crash incidents.

Old 48 Intel callback pointer rows and earlier Fortnite crash-lab witnesses remain
untouched. A database VIEW joins slots to their meanings without cloning the RVAs.
"""
import hashlib, json, pathlib, sqlite3, subprocess
from datetime import datetime, timezone

ROOT = pathlib.Path(__file__).resolve().parents[4]
REPO = ROOT / "09_REPOSITORY" / "underdog-d3d-compat"
DATA = REPO / "driver_intelligence"
DB = ROOT / "02_DATABASE" / "UNDERDOG_D3D_COMPAT_V01.sqlite"
LOCAL_HEADER = ROOT / "05_RESEARCH" / "INTEL_SDK_ADAPTER_ABI_V05_PROVENANCE.json"
LOCAL_FORTNITE = ROOT / "05_RESEARCH" / "INTEL_FORTNITE_GPU_FAULT_CORRELATION_V06.json"
PUBLIC_LAYOUT = DATA / "intel_hd520_adapter_abi_v05.json"
PUBLIC_FAULT = DATA / "intel_hd520_fortnite_gpu_faults_v06.json"
GAME_ROOT = pathlib.Path(r"C:\AIOS\04_WORKSPACES\PC_GAME_RE_LAB_01\GAMES\FORTNITE_UEFN")
PRIOR_CRASH = GAME_ROOT / "08_CRASH_LAB" / "GPU_HANG_20261108_01" / "CRASH_WITNESS_V01.json"
PRIOR_FIELD = GAME_ROOT / "09_GRAPHICS_COMPAT_LAB" / "measurements" / "FORTNITE_FIELDTEST_GPU_RESET_POSTMORTEM_V01.json"
OUTPUT = ROOT / "06_EVIDENCE" / "INTEL_ADAPTER_ABI_AND_FORTNITE_GPU_FAULT_V06.json"
DS = ROOT / "03_DATASET" / "INTEL_DRIVER_DDI_AND_GPU_FAULT_V06.json"
KNOW = ROOT / "04_KNOWLEDGE" / "INTEL_DDI_ABI_AND_FORTNITE_FAULT_V06.md"
LEDGER = ROOT / "04_KNOWLEDGE" / "DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
HANDOFF = ROOT / "08_HANDOFF" / "INTEL_DDI_LAYOUT_FORTNITE_FAULT_V06.json"

STAMP = datetime.now(timezone.utc).isoformat()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def json_load(p):
    raw=p.read_bytes()
    return json.loads(raw.decode("utf-16" if raw[:2] in (b"\xff\xfe",b"\xfe\xff") else "utf-8-sig"))

for p in (DB,LOCAL_HEADER,LOCAL_FORTNITE,PUBLIC_LAYOUT,PUBLIC_FAULT,PRIOR_CRASH,PRIOR_FIELD,LEDGER):
    if not p.is_file(): raise FileNotFoundError(p)
p=subprocess.run(["python",str(REPO/"tools/driver_intelligence/validate_intel_adapter_abi_v05.py")],capture_output=True,text=True)
assert p.returncode==0,p.stderr
p=subprocess.run(["python",str(REPO/"tools/driver_intelligence/validate_intel_fortnite_gpu_faults_v06.py")],capture_output=True,text=True)
assert p.returncode==0,p.stderr
layout=json_load(PUBLIC_LAYOUT)
fault=json_load(PUBLIC_FAULT)
prov=json_load(LOCAL_HEADER)
local=json_load(LOCAL_FORTNITE)
assert prov["upstream"]["status"]=="TWO_PINNED_SDK_HEADER_VERSIONS_VERIFIED"
assert prov["slots"]==8 and prov["rvas_mapped"]==48
assert tuple(x["git_blob_sha1"] for x in prov["upstream"]["sources"])==tuple(x["git_blob_sha1"] for x in layout["sdk_provenance"])
assert len(local["reporting_events_dred2"])==2
assert len(local["system_display_recoveries_4101"])==10
assert len(local["game_incidents"])==3
assert fault["root_cause"]=="UNKNOWN"
prior=json_load(PRIOR_CRASH)
post=json_load(PRIOR_FIELD)
assert prior["scope"]=="pc-game-re-modding / FORTNITE_UEFN"
assert post["status"]=="FAILED_GPU_STABILITY"
assert "earlier" in post["new_failure_is_distinct_from_old"].lower()
assert "intel_hd520_openadapter_v02.json" in layout["original_rva_map"]
sdk_git_blob=layout["sdk_provenance"][1]["git_blob_sha1"]
with sqlite3.connect(DB) as con:
    con.execute("PRAGMA foreign_keys=ON")
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    before_slots=con.execute("SELECT COUNT(*) FROM driver_adapter_slots").fetchone()[0]
    assert before_slots==48
    con.execute("""CREATE TABLE IF NOT EXISTS driver_adapter_abi_semantics (
        layout_version TEXT NOT NULL, slot_offset INTEGER NOT NULL,
        semantic_name TEXT NOT NULL, callback_type TEXT NOT NULL,
        abi_evidence_level TEXT NOT NULL, sdk_blob_sha1 TEXT NOT NULL,
        provenance_path TEXT NOT NULL, verified_utc TEXT NOT NULL,
        PRIMARY KEY(layout_version,slot_offset))""")
    for slot in layout["slots"]:
        off=int(slot["offset"],16)
        old=con.execute("""SELECT semantic_name, callback_type, sdk_blob_sha1
           FROM driver_adapter_abi_semantics
           WHERE layout_version=? AND slot_offset=?""",("v05",off)).fetchone()
        wanted=(slot["name"],slot["type"],sdk_git_blob)
        if old is not None and old!=wanted:
            raise RuntimeError("Semantic layout conflict at "+slot["offset"])
        con.execute("""INSERT OR IGNORE INTO driver_adapter_abi_semantics VALUES(?,?,?,?,?,?,?,?)""",
            ("v05",off,*wanted[:2],"WDK_SDK_TWO_VERSION_ABI_LAYOUT_VERIFIED",
             sdk_git_blob,str(LOCAL_HEADER),STAMP))
    con.execute("""CREATE VIEW IF NOT EXISTS driver_adapter_slots_semantic_v05 AS
      SELECT old.driver_version,old.handler_va,old.slot_offset,old.function_va,
             abi.semantic_name,abi.callback_type,abi.abi_evidence_level,
             old.semantic_label AS historical_underdog_label,
             old.evidence_path AS legacy_rva_evidence,abi.provenance_path AS sdk_provenance
      FROM driver_adapter_slots AS old
      JOIN driver_adapter_abi_semantics AS abi
        ON abi.layout_version='v05' AND old.slot_offset=abi.slot_offset""")
    joined=con.execute("SELECT COUNT(*) FROM driver_adapter_slots_semantic_v05").fetchone()[0]
    assert joined==before_slots==48
    assert con.execute("SELECT COUNT(*) FROM driver_adapter_abi_semantics WHERE layout_version='v05'").fetchone()[0]==8
    assert con.execute("SELECT semantic_name FROM driver_adapter_slots_semantic_v05 WHERE slot_offset=8 LIMIT 1").fetchone()[0]=="pfnCreateDevice"
    assert con.execute("SELECT semantic_name FROM driver_adapter_slots_semantic_v05 WHERE slot_offset=48 LIMIT 1").fetchone()[0]=="pfnFillDDITable"
    con.execute("""CREATE TABLE IF NOT EXISTS driver_gpu_fault_incidents (
      incident_id TEXT PRIMARY KEY, game_scope TEXT NOT NULL, hardware_id TEXT NOT NULL,
      reported_local_time TEXT NOT NULL, driver_version TEXT NOT NULL,
      removed_reason TEXT NOT NULL, removed_hresult TEXT NOT NULL,
      game_api_failure_types TEXT NOT NULL, pso_failure_count INTEGER NOT NULL,
      dred_enabled INTEGER NOT NULL, reported_budget_mb REAL,
      reported_local_used_mb REAL, evidence_path TEXT NOT NULL,
      first_original_crash_witness_path TEXT NOT NULL,
      driver_function_identity TEXT NOT NULL, confidence TEXT NOT NULL,
      admitted_utc TEXT NOT NULL)""")
    rows=[]
    for i,x in enumerate(fault["game_incidents"]):
        incident_id=("FRT-HD520-DRED-20261108-2121","FRT-HD520-DRED-20261108-2136",
                     "FRT-HD520-INTERNAL-20261109-0740")[i]
        api=json.dumps(x["d3d12_failure_indicators"],sort_keys=True)
        row=(incident_id,prior["scope"],fault["hardware_id"],x["time_local"],fault["driver"],
             x["device_removed_reason"],
             {"DXGI_ERROR_DEVICE_HUNG":"0x887A0006",
              "DXGI_ERROR_DRIVER_INTERNAL_ERROR":"0x887A0020"}[x["device_removed_reason"]],
             api,x["failed_pso_count"],0,
             x["reported_memory_mb"].get("local_budget_mb"),
             x["reported_memory_mb"].get("local_used_mb"),str(OUTPUT),
             str(PRIOR_CRASH),"UNKNOWN","GAME_LOG_WER_CORRELATED_NOT_CAUSAL",STAMP)
        existing=con.execute("SELECT removed_reason,game_api_failure_types FROM driver_gpu_fault_incidents WHERE incident_id=?",(incident_id,)).fetchone()
        if existing and existing!=(row[5],row[7]):
            raise RuntimeError("Incident classification drift without versioning "+incident_id)
        con.execute("INSERT OR IGNORE INTO driver_gpu_fault_incidents VALUES("+",".join(["?"]*17)+")",row)
        rows.append({"incident_id":incident_id,"local_time":row[3],"status":row[5]})
    assert con.execute("SELECT COUNT(*) FROM driver_gpu_fault_incidents WHERE game_scope=?",(prior["scope"],)).fetchone()[0]>=3
    assert con.execute("SELECT COUNT(*) FROM driver_adapter_slots").fetchone()[0]==48
    assert con.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    con.commit()
    counts={t:con.execute("SELECT COUNT(*) FROM "+t).fetchone()[0] for t in (
       "driver_adapter_slots","driver_adapter_abi_semantics","driver_adapter_slots_semantic_v05",
       "driver_gpu_fault_incidents","driver_callback_traces","driver_core_table_callbacks")}
# New semantic projection is distinct from old pointer facts, one 8-row table + 48-row view.
report={
 "schema":"aios.underdog.intel_hd520.ddi_semantics_fortnite_correlation.v6",
 "utc":STAMP,"hardware":"8086:1916","driver":"22.20.16.4836",
 "adapter_abi_slot_count":8,"adapter_rva_pointer_count":48,
 "adapter_semantic_source_sha256":sha(PUBLIC_LAYOUT),
 "pinned_sdk_source_evidence_sha256":sha(LOCAL_HEADER),
 "pinned_sdk_headers_verification":"TWO_BLOB_SHA_VERIFIED_ABI_LAYOUTS_IDENTICAL",
 "existing_create_device_evidence":"driver_intelligence/intel_hd520_createdevice_v03.json",
 "existing_pso_resource_evidence":"driver_intelligence/intel_hd520_pso_resource_v04.json",
 "fortnite_incident_count":3,"fortnite_wer_dred2_count":2,
 "igfx_recoveries_4101_count":10,
 "fortnite_local_evidence_sha256":sha(LOCAL_FORTNITE),
 "fortnite_public_summary_sha256":sha(PUBLIC_FAULT),
 "earlier_fortnite_crash_witness_sha256":sha(PRIOR_CRASH),
 "earlier_fortnite_postmortem_sha256":sha(PRIOR_FIELD),
 "database_counts":counts,
 "runtime_driver_callback_fault":"UNKNOWN",
 "memory_exhaustion_as_cause":"NOT_SUPPORTED_BY_REPORTED_BUDGET_VS_USED",
 "events":rows,"source_semantics":"SDK_LAYOUT_VERIFIED_NOT_LIVE_DRIVER_CALLBACK_TRACED",
 "privacy":"No private file paths, dump data, game logs or Intel binaries in GitHub dataset.",
 "next_gate":"Read-only inspection of original DRED/witness dumps and bounded DXGI/PSO/resource/queue ETW on authorized fixtures; no claim that post-hang API failures caused the TDR."
}
OUTPUT.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
DS.write_text(json.dumps({
 "schema":"aios.underdog.intel_hd520.ddi_fault_dataset.v6",
 "purpose":"Versioned semantic layout and correlated device-removal facts. No duplication of 48 RVAs.",
 "api_semantic_layout":"09_REPOSITORY/underdog-d3d-compat/driver_intelligence/intel_hd520_adapter_abi_v05.json",
 "original_address_map":"09_REPOSITORY/underdog-d3d-compat/driver_intelligence/intel_hd520_openadapter_v02.json",
 "fortnite_fault_projection":"09_REPOSITORY/underdog-d3d-compat/driver_intelligence/intel_hd520_fortnite_gpu_faults_v06.json",
 "existing_fortnite_scope":"pc-game-re-modding / FORTNITE_UEFN",
 "database_view":"driver_adapter_slots_semantic_v05","test_status":"ABI_MATCH_AND_WER_CORRELATION_PASS",
 "real_driver_function_causality":"UNRESOLVED","evidence":str(OUTPUT)},indent=2)+"\n",encoding="utf-8")
KNOW.write_text("# Intel HD 520 adapter ABI and Fortnite D3D12 fault correlation V06\n\n"
 "## ABI finding (source verified)\n"
 "Two pinned Windows SDK versions 10.0.10240.0 and 10.0.14393.0 contain identical 8-member D3D12DDI_ADAPTERFUNCS table layouts. "
 "Our previously recovered 5 installed driver variants plus one older variant contain 48 corresponding pointer RVAs in total. "
 "CreateDevice (+0x08) and FillDDITable (+0x30) have additional high-confidence static code traces. "
 "No user-mode driver callback was proven live in this cycle. This is a semantic ABI projection, not a driver patch.\n\n"
 "## Fortnite incidents (read-only WER + Unreal logs)\n"
 "Two Windows D3DDRED2 reports identify FortniteClient-Win64-Shipping.exe and Intel 22.20.16.4836 with DXGI_ERROR_DEVICE_HUNG (0x887A0006). "
 "Three historical Fortnite crash logs match these times plus a November 9 DRIVER_INTERNAL_ERROR (0x887A0020). "
 "Within the bounded window, ten System Display/igfx Event ID 4101 recovery records were found. "
 "Fortnite's crash contexts show RHI.DRED=false, GPU logs show Intel breadcrumbs disabled and EXISTING_HEAPS unavailable; thus GPU breadcrumb/page-fault traces are missing. "
 "Observed failing calls include CreatePlacedResource, CreatePipelineState and Resource->Map, but these can be *consequences* of prior removal. "
 "Memory Used (631.32/623.20/209.35MB) stayed below the reported budget (5480.06MB); this does not support a simple reported-budget exhaustion theory. "
 "The exact Intel driver callback initiating the hang is still UNKNOWN.\n\n"
 "## Data governance\n"
 "The 48-row pointer dataset is preserved unchanged. The new 8-row semantic table is joined through a SQLite view, not copied 48 more times. "
 "Existing Fortnite crash evidence remains in its earlier pc-game-re-modding / FORTNITE_UEFN scope. "
 "No raw crash dumps, driver binaries or user logs copied to GitHub. "
 f"Evidence: {OUTPUT}\nOriginal crash witness: {PRIOR_CRASH}\n",encoding="utf-8")
experience=[
 {"id":"INTEL-HD520-ADAPTER-ABI-V05","utc":STAMP,"kind":"sdk_layout_validation",
  "result":"8_ABI_SLOT_NAMES_VERIFIED_2_PINNED_SDK_HEADERS_48_POINTERS_JOINED",
  "confidence":"SDK_SOURCE_VERIFIED; CREATEDEVICE_AND_FILLDDI_STATIC_SIGNATURE",
  "evidence":str(OUTPUT)},
 {"id":"FORTNITE-HD520-DRED-CORRELATION-V06","utc":STAMP,"kind":"gpu_fault_correlation",
  "result":"2_D3DDRED2_DEVICE_HUNG_3_GAME_CRASHES_10_IGFX_4101_EVENTS",
  "confidence":"WINDOWS_WER_GAME_LOG_CORRELATED_CAUSE_UNRESOLVED",
  "evidence":str(OUTPUT),"next":"DRED GPU trace absent; no Intel driver callback proven causal"}
]
existing=[json.loads(l) for l in LEDGER.read_text(encoding="utf-8").splitlines() if l.strip()]
ids={x.get("id") for x in existing}
with LEDGER.open("a",encoding="utf-8") as f:
    for row in experience:
        if row["id"] not in ids:f.write(json.dumps(row)+"\n")
HANDOFF.write_text(json.dumps({
 "schema":"aios.underdog.intel_hd520.driver_handoff.v06",
 "timestamp_utc":STAMP,"scope":"pc-game-re-modding / D3D_COMPAT_UNDERDOG_01",
 "parent_fortnite_scope":"pc-game-re-modding / FORTNITE_UEFN",
 "status":"ADAPTER_SLOTS_RESOLVED; FORTNITE_GPU_RESET_CONFIRMED; CULPRIT_DRIVER_FUNCTION_UNRESOLVED",
 "source_abi":"10.0.10240.0 and 10.0.14393.0 pinned SDK mirror verified",
 "resolved_adapter_slots":8,"source_rva_map_entries":48,
 "adapter_signature_evidence":"CreateDevice and FillDDITable static code matched",
 "fortnite_dred_incidents":2,"fortnite_crash_logs":3,"display_driver_recoveries":10,
 "root_cause":"UNKNOWN","strict_qualification":"Post-device-removal API errors do not determine first faulting GPU operation",
 "database":str(DB),"database_view":"driver_adapter_slots_semantic_v05",
 "dataset":str(DS),"knowledge":str(KNOW),"evidence":str(OUTPUT),
 "local_provenance":[str(LOCAL_HEADER),str(LOCAL_FORTNITE)],
 "repo_branch":"feature/hd520-dx12-feature-probe-v01",
 "next":"Use read-only DRED/minidump/ETW evidence to identify preceding GPU command queue work. Avoid game hooks, driver patches and protected-process modification."
},indent=2)+"\n",encoding="utf-8")
print(json.dumps({"status":"ABI_AND_FORTNITE_FAULT_INGESTED_V06","db_counts":counts,"evidence":str(OUTPUT)},indent=2))
