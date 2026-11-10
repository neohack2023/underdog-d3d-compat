#!/usr/bin/env python3
"""Read-only bounded PE graphics-API inventory for current Host 02 game trees."""
import argparse
import json
import os
from datetime import datetime, timezone
from pathlib import Path
try:
    import pefile
except ImportError:
    pefile = None

ROOTS = ("03_RUNTIME", "01_EXTRACTED", "02_EXPERIMENTS")
SKIP_DIRS = {".git", "node_modules", "__pycache__", "cache", "caches", "backup", "backups"}
SHADER_EXT = {".fx", ".fxh", ".hlsl", ".cso", ".dxbc", ".dxil", ".vsh",
              ".psh", ".glsl", ".frag", ".vert", ".spv", ".shader", ".cg"}
API_DLL = {
    "ddraw.dll": "DirectDraw_legacy",
    "d3dim.dll": "Direct3D_legacy",
    "d3d8.dll": "D3D8",
    "d3d9.dll": "D3D9",
    "d3d10.dll": "D3D10",
    "d3d11.dll": "D3D11",
    "d3d12.dll": "D3D12",
    "dxgi.dll": "DXGI",
    "opengl32.dll": "OpenGL",
    "vulkan-1.dll": "Vulkan",
    "d3dcompiler_43.dll": "D3DCompiler",
    "d3dcompiler_47.dll": "D3DCompiler",
}


def scan_exe(path: Path):
    result = {"relative_path": str(path), "bytes": path.stat().st_size,
              "graphics_imports": [], "apis": [], "parse_status": "unparsed"}
    if pefile is None:
        result["parse_status"] = "pefile_unavailable"
        return result
    try:
        if path.stat().st_size > 160 * 1024 * 1024:
            result["parse_status"] = "skip_oversized"
            return result
        obj = pefile.PE(str(path), fast_load=True)
        try:
            obj.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
            dlls = sorted({imp.dll.decode("ascii", "replace").lower()
                           for imp in getattr(obj, "DIRECTORY_ENTRY_IMPORT", [])})
            result["graphics_imports"] = [name for name in dlls if name in API_DLL]
            result["apis"] = sorted({API_DLL[name] for name in result["graphics_imports"]})
            result["parse_status"] = "ok"
        finally:
            obj.close()
    except (OSError, ValueError, pefile.PEFormatError) as exc:
        result["parse_status"] = type(exc).__name__
    return result


def scan_game(game: Path, max_dirs: int, max_exes: int):
    candidates, shader_files = [], []
    n_dirs = 0
    truncated = False
    for sub in ROOTS:
        root = game / sub
        if not root.is_dir():
            continue
        for base, dirs, files in os.walk(root):
            n_dirs += 1
            if n_dirs > max_dirs:
                truncated = True
                break
            depth = len(Path(base).relative_to(root).parts)
            dirs[:] = sorted(d for d in dirs if d.lower() not in SKIP_DIRS) if depth < 4 else []
            for fn in sorted(files):
                ext = Path(fn).suffix.lower()
                file = Path(base) / fn
                if ext in SHADER_EXT and len(shader_files) < 30:
                    shader_files.append({"path": str(file.relative_to(game)), "extension": ext})
                elif ext == ".exe" and len(candidates) < max_exes:
                    candidates.append(file)
        if truncated or len(candidates) >= max_exes:
            break
    exes = []
    for exe in candidates:
        item = scan_exe(exe)
        item["relative_path"] = str(exe.relative_to(game))
        exes.append(item)
    apis = sorted({api for item in exes for api in item["apis"]})
    if not exes:
        finding = "no_executable_found_within_bounded_search"
    elif not apis:
        finding = "no_direct_graphics_import_detected"
    else:
        finding = "graphics_api_import_candidates"
    return {"game": game.name, "examined_directories": min(n_dirs, max_dirs),
            "truncated": truncated, "executables": exes,
            "shader_file_samples": shader_files, "observed_api_imports": apis,
            "finding": finding, "shaderlab_compatibility": "not_tested"}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--games-root", type=Path, required=True)
    ap.add_argument("--max-dirs", type=int, default=250)
    ap.add_argument("--max-exes", type=int, default=8)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    if not args.games_root.is_dir():
        ap.error("Games root not found")
    if not 1 <= args.max_dirs <= 1000 or not 1 <= args.max_exes <= 20:
        ap.error("bounds invalid")
    games = []
    for game in sorted(args.games_root.iterdir()):
        if game.is_dir():
            rec = scan_game(game, args.max_dirs, args.max_exes)
            print(game.name + ": " + ",".join(rec["observed_api_imports"]) +
                  " | executables=" + str(len(rec["executables"])), flush=True)
            games.append(rec)
    report = {"schema": "underdog.shaderlab.static_game_graphics_audit.v1",
              "captured_utc": datetime.now(timezone.utc).isoformat(),
              "game_count": len(games), "games": games,
              "limitations": [
                  "Import tables can omit graphics DLLs loaded dynamically",
                  "A native executable may be a launcher or modified clone",
                  "No executable was launched and no game files were changed",
                  "Shader files are listed by path only; no copyrighted data ingested",
                  "API imports do not establish rendering compatibility or gameplay"
              ]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("REPORT=" + str(args.output))


if __name__ == "__main__":
    main()
