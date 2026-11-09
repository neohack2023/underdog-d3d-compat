"""Static Intel Gen9 PSO/resource callback map regression; no GPU required."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[2]
data=json.loads((root/"driver_intelligence"/"intel_hd520_pso_resource_v04.json").read_text(encoding="utf8"))
assert data["schema"]=="underdog.intel_gen9.device_core_pso_resource_static_map.v4"
assert data["driver_version"]=="22.20.16.4836"
assert data["adapter_filled_table_slot"]=="0x30"
assert data["fill_ddi_wrapper_rva"]=="0x400100"
assert data["fill_ddi_implementation_rva"]=="0x1225b0"
assert data["ddi_slot_offsets"]=={"CreatePipelineState":"0x110","CreateHeapAndResource":"0x1e0"}
expected={
 "0x1234a0":("0xc0002",712,"0x400c00","0x401120"),
 "0x122fb0":("0xc000a",736,"0x400bc0","0x401300"),
 "0x121730":("0xc0014",736,"0x400c40","0x401300"),
 "0x122020":("0xc001e",736,"0x400ca0","0x4014c0")
}
assert len(data["variants"])==4
for v in data["variants"]:
 assert v["table_constructor_rva"] in expected
 exp=expected[v["table_constructor_rva"]]
 assert (v["observed_ddi_code"],v["size_bytes"])==exp[:2]
 assert v["pointer_slots_initialized"] in (88,91)
 assert v["exact_version_name"]=="UNRESOLVED"
 assert set(v["callbacks"])=={"CreatePipelineState","CreateHeapAndResource"}
 assert (v["callbacks"]["CreatePipelineState"]["wrapper_rva"],
         v["callbacks"]["CreateHeapAndResource"]["wrapper_rva"])==exp[2:]
 for semantic,slot in data["ddi_slot_offsets"].items():
  row=v["callbacks"][semantic]
  assert row["slot_offset"]==slot
  assert 0<int(row["implementation_rva"],16)<0x500000
  assert row["confidence"]=="HIGH_STATIC_LAYOUT_CORRELATION_NOT_LIVE_DDI"
assert any("API-level failure" in note for note in data["boundary_qualifications"])
print("INTEL_DDI_PSO_RESOURCE_MAP_V04_PASS four tables eight callbacks")
