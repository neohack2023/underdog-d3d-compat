from pathlib import Path
import json
root=Path(__file__).resolve().parents[2]
d=json.loads((root/"driver_intelligence"/"intel_hd520_uav_binding_v05.json").read_text(encoding="utf8"))
assert d["schema"]=="underdog.intel_gen9.typed_uav_binding_regression.v05"
assert d["hardware"]["pci_id"]=="8086:1916"
assert d["format_modes"]==["R32G32B32A32_FLOAT","R8G8B8A8_UNORM"]
assert d["repetitions_per_mode"]==3 and d["accepted_matrix_runs"]==6
assert d["typed_root"]["pso_hresult"]=="0x80070057"
assert d["typed_root"]["gpu_dispatch"]=="NOT_SUBMITTED"
assert d["typed_table"]["pso_hresult"]=="0x00000000"
assert d["typed_table"]["pixel_validation"]=="PASS"
assert d["structured_root"]["pixel_validation"]=="PASS"
assert d["structured_table"]["pixel_validation"]=="PASS"
assert d["rgba8_bytes"]=="64,128,191,255"
assert len(d["source_sha256"])==len(d["binary_sha256"])==len(d["local_evidence_sha256"])==64
print("UNDERDOG_TYPED_UAV_BINDING_V05_METADATA_PASS")
