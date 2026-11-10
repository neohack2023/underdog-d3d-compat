#!/usr/bin/env python3
"""Bounded, opt-in HD520 A/B replay of owned Underdog Arena shaders."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import re
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCENES = ("cube_12tri.obj", "grid_800tri.obj", "grid_3200tri.obj")
EXE = Path("build/Release/underdog_arena_v03.exe")


def capture(text: str, key: str):
    m = re.search(r"(?:^|\s)" + re.escape(key) + r"=([^\s]+)", text)
    return m.group(1) if m else None


def trial(executable: Path, scene: str, backend: str, level: str) -> dict:
    with tempfile.TemporaryDirectory(prefix="udog_ab_witness_") as temp:
        env = os.environ.copy()
        env["UNDERDOG_SHADERLAB_VARIANT"] = level
        env["UNDERDOG_SHADERLAB_WITNESS_DIR"] = temp
        command = [str(executable), "60", "--autoplay", "fixtures/" + scene]
        if backend == "typed":
            command.append("--typed-uav")
        try:
            run = subprocess.run(command, cwd=str(ROOT), env=env, capture_output=True,
                                 text=True, errors="replace", timeout=110)
        except subprocess.TimeoutExpired:
            return {"scene": scene, "backend": backend, "variant": level,
                    "ok": False, "failure": "subprocess_timeout"}
        out = run.stdout + "\n" + run.stderr
        witness = capture(out, "GPU_WITNESS_PATH")
        image = Path(witness) if witness else None
        image_valid = bool(image and image.is_file() and image.resolve().is_relative_to(Path(temp).resolve()))
        sha = hashlib.sha256(image.read_bytes()).hexdigest() if image_valid else None
        result = {
            "scene": scene, "backend": backend, "variant": level, "exit_code": run.returncode,
            "reported_variant": capture(out, "SHADERLAB_VARIANT"),
            "frames": capture(out, "SWAPCHAIN_FRAMES"),
            "presents": capture(out, "PRESENT_SUCCESS"),
            "witness_valid": capture(out, "WITNESS_VALID"),
            "image_sha256": sha,
            "gpu_p50_ms": capture(out, "GPU_BVH_COPY_P50_MS"),
            "gpu_p95_ms": capture(out, "GPU_BVH_COPY_P95_MS"),
            "device_removed_reason": capture(out, "DEVICE_REMOVED_REASON"),
            "stdout_tail": out[-3500:],
        }
        result["ok"] = (run.returncode == 0 and result["reported_variant"] == level
                        and result["frames"] == "60" and result["presents"] == "60"
                        and result["witness_valid"] == "1" and image_valid
                        and result["device_removed_reason"] == "0x00000000")
        return result


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--repeats", type=int, default=2)
    ap.add_argument("--scenes", nargs="*", choices=SCENES, default=list(SCENES))
    args = ap.parse_args()
    if args.repeats < 1 or args.repeats > 3:
        ap.error("repeats must be 1..3")
    exe = (ROOT / EXE).resolve()
    if not exe.is_file():
        ap.error("Build Arena V03 first")
    report = {"schema": "underdog.shaderlab.hd520_gpu_ab.v1",
              "captured_utc": datetime.now(timezone.utc).isoformat(),
              "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
              "scenes": args.scenes, "repeats": args.repeats, "runs": [], "pairs": []}
    stop = False
    for scene in args.scenes:
        for backend in ("structured", "typed"):
            for rep in range(args.repeats):
                pair = []
                for level in ("O1", "O3"):
                    item = trial(exe, scene, backend, level)
                    item["repeat"] = rep + 1
                    report["runs"].append(item)
                    pair.append(item)
                    print(f"{scene} {backend} repeat={rep+1} {level}: "
                          f"ok={item['ok']} gpu_p50={item.get('gpu_p50_ms')} "
                          f"image={str(item.get('image_sha256'))[:12]}", flush=True)
                    if not item["ok"]:
                        stop = True
                        break
                if len(pair) == 2:
                    equal = pair[0]["image_sha256"] == pair[1]["image_sha256"]
                    report["pairs"].append({
                        "scene": scene, "backend": backend, "repeat": rep + 1,
                        "both_ok": all(x["ok"] for x in pair),
                        "byte_identical": equal,
                        "o1_p50_ms": pair[0]["gpu_p50_ms"], "o3_p50_ms": pair[1]["gpu_p50_ms"],
                    })
                if stop:
                    break
            if stop:
                break
        if stop:
            break
    report["runs_passed"] = sum(bool(x["ok"]) for x in report["runs"])
    report["pairs_byte_identical"] = sum(bool(x["byte_identical"]) for x in report["pairs"])
    report["all_accepted"] = (not stop and len(report["pairs"]) == len(args.scenes) * 2 * args.repeats
                              and all(x["both_ok"] and x["byte_identical"] for x in report["pairs"]))
    report["performance_conclusion"] = "None: paired repeated measurements require distribution analysis, do not auto-promote O3"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"REPORT={args.output} accepted={report['all_accepted']} "
          f"runs={report['runs_passed']}/{len(report['runs'])} "
          f"identical_pairs={report['pairs_byte_identical']}/{len(report['pairs'])}", flush=True)
    return 0 if report["all_accepted"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
