"""Checks source-free Intel Gen9 driver interface map. No host driver needed."""
from pathlib import Path
import json
p=Path(__file__).resolve().parent.parent.parent/"driver_intelligence"/"intel_hd520_openadapter_v02.json"
x=json.loads(p.read_text(encoding="utf8"))
assert x["schema"]=="underdog.intel_gen9.adapter_address_map.v2"
assert x["hardware"]["pci"]=="8086:1916"
offsets={hex(8*i) for i in range(8)}
assert set(x["slots"]["offsets"])==offsets
assert {hex(int(k)) for k in x["old"]["slots"]}==offsets
assert len(x["installed"]["variants"])==5
assert len(x["installed"]["case_to_implementation"])==14
assert sorted(k for k,v in x["installed"]["case_to_implementation"].items() if v=="E_INVALIDARG")==["0x15","0x19"]
seen={}
for row in x["installed"]["variants"]:
 assert set(row["slots"])==offsets
 for key,value in row["slots"].items():
  assert int(value,16)>0 and int(value,16)<0x10000000
 for code in row["query_codes"]:
  assert code not in seen
  seen[code]=row["implementation_rva"]
assert len(seen)==12
assert all(x["installed"]["case_to_implementation"][k]==v for k,v in seen.items())
assert x["slots"]["semantic_names"]=="UNRESOLVED"
print("INTEL_ADAPTER_DDI_MAP_SCHEMA_PASS 6 variants 48 callback slots 14 dispatch codes")
