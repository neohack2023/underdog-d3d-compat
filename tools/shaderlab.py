#!/usr/bin/env python3
"""Underdog ShaderLab V01: source-owned HLSL inventory and deterministic compiler matrix.
No game injection, driver modification, or persistent bytecode archive.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ARENA = Path("examples/games/underdog_arena_v03.cpp")
STRUCT_DECL = "RWStructuredBuffer<uint> rgba:register(u0);"
STRUCT_STORE = "rgba[tid.y*128+tid.x]=rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xFF000000;"
TEX_DECL = "RWTexture2D<float4> rgba:register(u0);"
TEX_STORE = "rgba[tid.xy]=float4(float3(rgb)/255.0,1.0);"
SDK_BIN = Path(r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64")


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract_arena(repo: Path) -> dict[str, str]:
    source = (repo / ARENA).read_text(encoding="utf-8-sig")
    match = re.search(r'const char\* shader=R"SHADER\((.*?)\)SHADER";', source, re.DOTALL)
    if not match:
        raise ValueError("Arena embedded HLSL source marker missing")
    baseline = match.group(1)
    if baseline.count(STRUCT_DECL) != 1 or baseline.count(STRUCT_STORE) != 1:
        raise ValueError("Arena shader structure changed: manual review required")
    typed = baseline.replace(STRUCT_DECL, TEX_DECL).replace(STRUCT_STORE, TEX_STORE)
    if typed.count(TEX_DECL) != 1 or typed.count(TEX_STORE) != 1:
        raise ValueError("Typed UAV variant not uniquely derived")
    return {"structured": baseline, "typed": typed}


def manifest(repo: Path) -> dict:
    shaders = extract_arena(repo)
    return {
        "schema": "underdog.shaderlab.source_inventory.v1",
        "source_path": ARENA.as_posix(),
        "source_cpp_sha256": sha((repo / ARENA).read_bytes()),
        "variants": {
            key: {"sha256": sha(value.encode("utf-8")), "bytes": len(value.encode("utf-8")),
                  "target": "cs_5_1", "entrypoint": "main",
                  "binding": ("root_structured_uav" if key == "structured" else "descriptor_table_typed_uav")}
            for key, value in shaders.items()
        },
        "provenance": "Extracted from original source-owned C++ shader; no third-party binary",
    }


def compile_matrix(repo: Path, fxc: Path, dxc: Path | None) -> dict:
    if not fxc.is_file():
        raise FileNotFoundError(f"FXC executable unavailable: {fxc}")
    result = manifest(repo)
    result.update({"schema": "underdog.shaderlab.compiler_matrix.v1",
                   "captured_utc": datetime.now(timezone.utc).isoformat(),
                   "fxc_path": str(fxc), "fxc_sha256": sha(fxc.read_bytes()),
                   "dxc_path": str(dxc) if dxc and dxc.is_file() else None,
                   "results": []})
    with tempfile.TemporaryDirectory(prefix="underdog_shaderlab_") as d:
        temp = Path(d)
        for name, source in extract_arena(repo).items():
            src = temp / f"arena_{name}.hlsl"
            src.write_text(source, encoding="utf-8", newline="\n")
            for level in ("O1", "O3"):
                binary = temp / f"{name}_{level}.cso"
                command = [str(fxc), "/nologo", "/T", "cs_5_1", "/E", "main",
                           f"/{level}", "/Fo", str(binary), str(src)]
                try:
                    run = subprocess.run(command, capture_output=True, text=True,
                                         timeout=90, errors="replace")
                    ok = run.returncode == 0 and binary.is_file()
                    record = {"shader": name, "optimization": level, "compiler": "FXC",
                              "target": "cs_5_1", "exit_code": run.returncode,
                              "ok": ok, "diagnostics": (run.stdout + run.stderr)[-3000:]}
                    if ok:
                        payload = binary.read_bytes()
                        record.update({"bytecode_sha256": sha(payload), "bytecode_bytes": len(payload)})
                except subprocess.TimeoutExpired:
                    record = {"shader": name, "optimization": level, "compiler": "FXC",
                              "target": "cs_5_1", "ok": False, "error": "compiler_timeout"}
                result["results"].append(record)
        # DXC SM6 compilation is offline-only. A valid DXIL binary does not imply HD520 support.
        if dxc and dxc.is_file():
            for name, source in extract_arena(repo).items():
                src = temp / f"arena_{name}_dxc.hlsl"
                src.write_text(source, encoding="utf-8", newline="\n")
                binary = temp / f"{name}_dxc.dxil"
                try:
                    run = subprocess.run([str(dxc), "-T", "cs_6_0", "-E", "main",
                                          "-Fo", str(binary), str(src)],
                                         capture_output=True, text=True, timeout=90, errors="replace")
                    ok = run.returncode == 0 and binary.is_file()
                    record = {"shader": name, "optimization": "default", "compiler": "DXC",
                              "target": "cs_6_0", "offline_only": True,
                              "exit_code": run.returncode, "ok": ok,
                              "diagnostics": (run.stdout + run.stderr)[-3000:]}
                    if ok:
                        payload = binary.read_bytes()
                        record.update({"bytecode_sha256": sha(payload), "bytecode_bytes": len(payload)})
                except subprocess.TimeoutExpired:
                    record = {"shader": name, "compiler": "DXC", "target": "cs_6_0",
                              "offline_only": True, "ok": False, "error": "compiler_timeout"}
                result["results"].append(record)
    result["all_compiled"] = all(row["ok"] for row in result["results"])
    result["note"] = "Bytecode files were temporary. Shader execution and performance are NOT proven by compilation."
    return result


def game_inventory(games_root: Path) -> dict:
    if not games_root.is_dir():
        raise FileNotFoundError(games_root)
    games = []
    for p in sorted(games_root.iterdir(), key=lambda x: x.name.lower()):
        if not p.is_dir():
            continue
        entries = {child.name for child in p.iterdir() if child.is_dir()}
        games.append({"game": p.name, "game_dir_present": True,
                      "has_extracted_tree": "01_EXTRACTED" in entries,
                      "has_runtime_tree": "03_RUNTIME" in entries,
                      "shaderlab_integration": "not_tested",
                      "classification": "candidate_inventory_only"})
    return {"schema": "underdog.shaderlab.host02_game_inventory.v1",
            "captured_utc": datetime.now(timezone.utc).isoformat(),
            "games_root": str(games_root),
            "games": games, "note": "Folder presence is not evidence of game launch or ShaderLab compatibility."}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("inventory", "compile", "games"))
    parser.add_argument("--repo", type=Path, default=ROOT)
    parser.add_argument("--fxc", type=Path, default=SDK_BIN / "fxc.exe")
    parser.add_argument("--dxc", type=Path, default=SDK_BIN / "dxc.exe")
    parser.add_argument("--games-root", type=Path)
    parser.add_argument("--output", type=Path)
    a = parser.parse_args()
    if a.mode == "inventory":
        report = manifest(a.repo)
    elif a.mode == "compile":
        report = compile_matrix(a.repo, a.fxc, a.dxc)
    else:
        if a.games_root is None:
            parser.error("--games-root required for games")
        report = game_inventory(a.games_root)
    body = json.dumps(report, sort_keys=True, indent=2) + "\n"
    if a.output:
        a.output.parent.mkdir(parents=True, exist_ok=True)
        a.output.write_text(body, encoding="utf-8")
        print(f"REPORT={a.output}")
    print(body)
    return 0 if a.mode != "compile" or report["all_compiled"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
