#!/usr/bin/env python3
"""Independent synthetic GS PSMCT32 sprite inputs; GPL-free reproduction aid.

Ground truth: byte-checked against actual PS2Recomp GSCpuBackend CPU witness.
Does not implement or claim to replace the full GS renderer.
"""
import argparse
import json
from pathlib import Path

W = H = 64
TW = TH = 8
EXPECTED_IMAGE_FNV64 = "fd73c2725d347ea5"
EXPECTED_TEXTURE_FNV64 = "4560c51eca146805"


def fnv64(data: bytes) -> str:
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return f"{value:016x}"


def build() -> tuple[bytes, bytes]:
    texture = bytearray(TW * TH * 4)
    for y in range(TH):
        for x in range(TW):
            i = (y * TW + x) * 4
            texture[i : i + 4] = bytes((
                (x * 31 + y * 17 + 7) & 255,
                (y * 35 + x * 11 + 23) & 255,
                ((x ^ y) * 29 + 51) & 255,
                120 + ((x + y) & 7) * 15,
            ))
    framebuffer = bytearray(W * H * 4)
    for y in range(8, 40):
        for x in range(8, 40):
            tx, ty = (x - 8) // 4, (y - 8) // 4
            src = (ty * TW + tx) * 4
            dst = (y * W + x) * 4
            framebuffer[dst:dst + 4] = texture[src:src + 4]
    return bytes(texture), bytes(framebuffer)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path,
                        help="Isolated output directory; no repository files are changed")
    parser.add_argument("--verify-cpu", type=Path,
                        help="Optional actual PS2Recomp CPU RGBA witness to compare")
    args = parser.parse_args()
    texture, expected = build()
    if fnv64(texture) != EXPECTED_TEXTURE_FNV64:
        raise SystemExit("Texture checksum is incorrect")
    if fnv64(expected) != EXPECTED_IMAGE_FNV64:
        raise SystemExit("Expected image checksum is incorrect")
    cpu_match = None
    if args.verify_cpu:
        cpu_match = args.verify_cpu.read_bytes() == expected
        if not cpu_match:
            raise SystemExit("Generated reference does not equal actual CPU GS witness")
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "gs_texture_8x8.rgba").write_bytes(texture)
    (args.output / "gs_cpu_64x64.rgba").write_bytes(expected)
    print(json.dumps({
        "status": "PASS",
        "generated_image_fnv64": fnv64(expected),
        "generated_texture_fnv64": fnv64(texture),
        "equal_to_actual_cpu_gs_oracle": cpu_match,
        "output": str(args.output),
    }))


if __name__ == "__main__":
    main()
