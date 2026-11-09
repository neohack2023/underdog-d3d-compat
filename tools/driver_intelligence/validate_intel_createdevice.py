"""Validate CreateDevice static RVA mapping; no Windows SDK/GPU required."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[2]
base=json.loads((root/"driver_intelligence/intel_hd520_openadapter_v02.json").read_text(encoding="utf8"))
traces=json.loads((root/"driver_intelligence/intel_hd520_createdevice_v03.json").read_text(encoding="utf8"))
assert traces["schema"]=="underdog.intel_gen9.createdevice_static_map.v3"
assert traces["confidence"]=="HIGH_CONFIDENCE_STATIC_SIGNATURE_NOT_DYNAMIC"
assert traces["adapter_slot_offset"]=="0x08"
assert len(traces["dispatch_variants"])==5
variants={x["implementation_rva"]:x for x in base["installed"]["variants"]}
mapped=set()
for row in traces["dispatch_variants"]:
 assert row["variant_rva"] in variants
 assert row["slot_offset"]=="0x08"
 assert row["callback_wrapper_rva"]==variants[row["variant_rva"]]["slots"]["0x8"]
 assert set(row["dispatch_values"])==set(variants[row["variant_rva"]]["query_codes"])
 assert row["confidence"]=="HIGH_STATIC_DDI_SIGNATURE"
 assert int(row["implementation_rva"],16)>0
 mapped.update(row["dispatch_values"])
assert len(mapped)==12
assert set(traces["additional_call_chain"]["offsets"].values())=={"0x08","0x0c","0x20","0x28"}
print("INTEL_CREATEDEVICE_STATIC_TRACE_PASS 5 variants 12 dispatch codes")
