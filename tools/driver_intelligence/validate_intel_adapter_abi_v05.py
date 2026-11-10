#!/usr/bin/env python3
"""Correlate pinned 2015/2016 SDK D3D12 adapter ABI with existing Intel RVA datasets.

Default mode is deterministic/offline and suitable for CI. Optional --verify-upstream
fetches two version-pinned public SDK mirror headers into memory only, checks their
Git blob SHA-1 and the exact typedef member order. No WDK header is redistributed.
"""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import re
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "driver_intelligence"
NAMES = (
    ("0x00", "pfnCalcPrivateDeviceSize", "PFND3D12DDI_CALCPRIVATEDEVICESIZE"),
    ("0x08", "pfnCreateDevice", "PFND3D12DDI_CREATEDEVICE_0003"),
    ("0x10", "pfnCloseAdapter", "PFND3D12DDI_CLOSEADAPTER"),
    ("0x18", "pfnGetSupportedVersions", "PFND3D12DDI_GETSUPPORTEDVERSIONS"),
    ("0x20", "pfnGetCaps", "PFND3D12DDI_GETCAPS"),
    ("0x28", "pfnGetOptionalDDITables", "PFND3D12DDI_GETOPTIONALDDITTABLES"),
    ("0x30", "pfnFillDDITable", "PFND3D12DDI_FILLDDITTABLE"),
    ("0x38", "pfnDestroyDevice", "PFND3D12DDI_DESTROYDEVICE"),
)
PINNED_COMMIT = "9b69fd26ac0c7d0b83d378dba01080e93349c2ed"


def load(name):
    return json.loads((DATA / name).read_text(encoding="utf-8"))


def parse_header(data):
    text = data.decode("utf-8-sig")
    m = re.search(
        r"typedef\s+struct\s+D3D12DDI_ADAPTERFUNCS\s*\{(.*?)\}\s*D3D12DDI_ADAPTERFUNCS\s*;",
        text,
        re.S,
    )
    if not m:
        raise AssertionError("D3D12DDI_ADAPTERFUNCS typedef not found")
    parsed = re.findall(r"(PFND3D12DDI_[A-Z0-9_]+)\s+(pfn\w+)\s*;", m.group(1))
    expected = [(ptype, name) for _offset, name, ptype in NAMES]
    assert parsed == expected, {"actual": parsed, "expected": expected}
    return parsed


def validate_offline():
    layout = load("intel_hd520_adapter_abi_v05.json")
    base = load("intel_hd520_openadapter_v02.json")
    create = load("intel_hd520_createdevice_v03.json")
    core = load("intel_hd520_pso_resource_v04.json")
    assert layout["schema"] == "underdog.intel_gen9.adapter_semantic_layout.v5"
    assert layout["pci_id"] == base["hardware"]["pci"] == "8086:1916"
    assert layout["driver_versions"] == [base["versions"]["old"], base["versions"]["installed"]]
    assert len(layout["sdk_provenance"]) == 2
    assert {x["version"] for x in layout["sdk_provenance"]} == {"10.0.10240.0", "10.0.14393.0"}
    assert all(x["revision"] == PINNED_COMMIT for x in layout["sdk_provenance"])
    assert layout["pointer_size_x64_bytes"] == ctypes.sizeof(ctypes.c_void_p) == 8
    assert layout["table_size_x64_bytes"] == len(NAMES) * 8 == 64
    assert len(layout["slots"]) == 8
    assert [int(s["offset"], 16) for s in layout["slots"]] == list(range(0, 64, 8))
    assert tuple((s["offset"], s["name"], s["type"]) for s in layout["slots"]) == NAMES
    assert layout["original_rva_map"] == "driver_intelligence/intel_hd520_openadapter_v02.json"
    assert base["slots"]["semantic_names"] == "UNRESOLVED", "Historical V02 map must remain unchanged"
    assert len(base["installed"]["variants"]) == 5
    keyset = set(range(0, 64, 8))
    old = base["old"]["slots"]
    assert {int(k, 0) for k in old} == keyset
    for value in old.values():
        assert 0 < int(value, 16) < 0x10000000
    for variant in base["installed"]["variants"]:
        assert {int(k, 0) for k in variant["slots"]} == keyset
        assert len(set(variant["slots"].values())) == 8
        for value in variant["slots"].values():
            assert 0 < int(value, 16) < 0x10000000
    assert create["adapter_slot_offset"] == "0x08"
    variants = {v["implementation_rva"]: v for v in base["installed"]["variants"]}
    for v in create["dispatch_variants"]:
        row = variants[v["variant_rva"]]
        assert int(v["slot_offset"], 16) == 8
        assert v["callback_wrapper_rva"] == row["slots"]["0x8"]
    assert int(core["adapter_filled_table_slot"], 16) == 0x30
    fill = variants["0x1213b0"]["slots"]["0x30"]
    assert fill == core["fill_ddi_wrapper_rva"]
    assert len(base["installed"]["case_to_implementation"]) == 14
    assert sum(len(v["slots"]) for v in base["installed"]["variants"]) + len(old) == 48
    return {
        "status": "INTEL_ADAPTER_ABI_V05_OFFLINE_PASS",
        "slots": 8,
        "rvas_mapped": 48,
        "driver_variants": 6,
        "createdevice_slot": "0x08",
        "fillddi_slot": "0x30",
        "dynamic_driver_execution_proven": False,
    }


def verify_upstream():
    results = []
    layout = load("intel_hd520_adapter_abi_v05.json")
    for source in layout["sdk_provenance"]:
        url = (
            "https://raw.githubusercontent.com/tpn/winsdk-10/"
            + PINNED_COMMIT
            + "/"
            + source["path"]
        )
        req = urllib.request.Request(url, headers={"User-Agent": "UnderdogABI-Provenance/1.0"})
        with urllib.request.urlopen(req, timeout=24) as resp:
            if not resp.url.startswith("https://"):
                raise AssertionError("HTTPS required")
            raw = resp.read(900001)
            assert len(raw) <= 900000, "SDK mirror file exceeds hard cap"
        blob = hashlib.sha1(b"blob " + str(len(raw)).encode("ascii") + b"\0" + raw).hexdigest()
        assert blob == source["git_blob_sha1"], ("Pinned file differs", source["version"], blob)
        parsed = parse_header(raw)
        results.append(
            {
                "version": source["version"],
                "source": url,
                "git_blob_sha1": blob,
                "sha256": hashlib.sha256(raw).hexdigest(),
                "type_member_names": [item[1] for item in parsed],
                "status": "PINNED_SDK_TYPEDEF_VERIFIED",
                "headers_persisted_to_repo": False,
            }
        )
    assert results[0]["type_member_names"] == results[1]["type_member_names"]
    return {"status": "TWO_PINNED_SDK_HEADER_VERSIONS_VERIFIED", "sources": results}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify-upstream", action="store_true")
    args = parser.parse_args()
    report = validate_offline()
    if args.verify_upstream:
        report["upstream"] = verify_upstream()
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
