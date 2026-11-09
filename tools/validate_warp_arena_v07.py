"""No-GPU verification for the recorded original GPU witnesses."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
w=json.loads((root/"evidence/WARP_TYPED_UAV_AB_V06.json").read_text(encoding="utf8"))
a=json.loads((root/"evidence/ARENA_TEXTURE_BACKEND_PARITY_V07.json").read_text(encoding="utf8"))
assert w["schema"]=="underdog.warp.typed_uav_binding_ab.v06"
assert w["adapter"]=="Microsoft WARP DXGI 1414:008C"
assert w["matrix_count"]==6 and w["all_pass"]
assert w["typed_root_pso"]=="0x80070057" and w["typed_descriptor_table_pso"]=="0x00000000"
assert w["device_removed_reason"]=="0x00000000"
assert a["schema"]=="underdog.arena.typed_texture_backends.v07"
assert a["hardware"]["pci"]=="8086:1916"
assert a["run_count"]==12 and a["pair_count"]==a["exact_image_pairs"]==6
assert a["different_pixel_channels"]==0
assert {x["scene"] for x in a["pairs"]}=={"cube12","grid800","grid3200"}
assert sorted(x["repeat"] for x in a["pairs"])==[1,1,1,2,2,2]
assert all(x["frames_per_mode"]==60 and x["exact_pixel_parity"] and not x["mismatched_channels"] for x in a["pairs"])
assert all(x["device_removed_reason"]=="0x00000000" for x in a["pairs"])
png=root/a["visual_witness"]
assert png.read_bytes().startswith(bytes([137,80,78,71,13,10,26,10]))
assert hashlib.sha256(png.read_bytes()).hexdigest()==a["visual_witness_sha256"]
assert len(w["raw_evidence_sha256"])==len(a["raw_evidence_sha256"])==64
print("UNDERDOG_WARP_ARENA_V07_EVIDENCE_PASS")
