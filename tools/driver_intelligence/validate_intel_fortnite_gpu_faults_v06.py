#!/usr/bin/env python3
"""Source-free regression for Fortnite/HD520 DXGI device faults. No live GPU required."""
import json
from pathlib import Path
from datetime import datetime
root=Path(__file__).resolve().parents[2]
data=json.loads((root/"driver_intelligence"/"intel_hd520_fortnite_gpu_faults_v06.json").read_text(encoding="utf-8"))
assert data["schema"]=="underdog.intel_gen9.fortnite_gpu_faults.v6"
assert data["hardware_id"]=="host02-intel-hd-520"
assert data["driver"]=="22.20.16.4836"
assert data["dred2_fortnite_event_count"]==2
assert data["display_driver_recoveries_4101_count"]==10
assert len(data["game_incidents"])==3
assert data["root_cause"]=="UNKNOWN" and data["specific_intel_callback"]=="UNRESOLVED"
assert data["raw_log_data_distributed"] is False
assert data["proprietary_driver_or_game_binaries_distributed"] is False
assert data["local_evidence_file"]=="05_RESEARCH/INTEL_FORTNITE_GPU_FAULT_CORRELATION_V06.json"
seen=[]
for event in data["game_incidents"]:
    seen.append(datetime.fromisoformat(event["time_local"]))
    assert event["unreal_dred_enabled"] is False
    assert event["breadcrumbs_available"] is False
    assert event["reported_memory_mb"]["local_used_mb"] < event["reported_memory_mb"]["local_budget_mb"]
assert seen==sorted(seen)
assert [item["device_removed_reason"] for item in data["game_incidents"]]==[
    "DXGI_ERROR_DEVICE_HUNG","DXGI_ERROR_DEVICE_HUNG","DXGI_ERROR_DRIVER_INTERNAL_ERROR"]
assert data["game_incidents"][0]["d3d12_failure_indicators"]["create_placed_resource"] is True
assert data["game_incidents"][1]["d3d12_failure_indicators"]["pipeline_creation"] is True
assert data["game_incidents"][2]["d3d12_failure_indicators"]["resource_map"] is True
assert all(item["d3d12_failure_indicators"]["create_placed_resource"] is False
           for item in data["game_incidents"][2:])
assert all("C:\\" not in json.dumps(item) for item in data["game_incidents"])
print("INTEL_FORTNITE_GPU_FAULT_V06_PASS 3 correlated crashes 2 D3DDRED2 10 igfx recoveries; root cause unresolved")
