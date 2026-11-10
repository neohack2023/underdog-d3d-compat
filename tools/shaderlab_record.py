#!/usr/bin/env python3
"""Import verified local ShaderLab receipts into the existing Underdog SQLite DB.
Additive-only, receipt-keyed tables. Not a Drive authority promotion.
"""
import argparse
import hashlib
import json
import sqlite3
import statistics
from pathlib import Path


def load(path):
    data = Path(path).read_bytes()
    return json.loads(data), hashlib.sha256(data).hexdigest()


def record(db_path, tool_path, compiler_path, gpu_path, games_path, backup_path, ledger_path):
    if not db_path.is_file():
        raise FileNotFoundError(db_path)
    tools, t_sha = load(tool_path)
    compiler, c_sha = load(compiler_path)
    gpu, g_sha = load(gpu_path)
    games, a_sha = load(games_path)
    assert tools["schema"] == "aios.underdog.shaderlab.toolchain.v1"
    assert compiler["schema"] == "underdog.shaderlab.compiler_matrix.v1"
    assert gpu["schema"] == "underdog.shaderlab.hd520_gpu_ab.v1"
    assert games["schema"] == "underdog.shaderlab.static_game_graphics_audit.v1"
    if not compiler["all_compiled"] or not gpu["all_accepted"]:
        raise ValueError("Evidence acceptance failed; not recording as verified")
    backup_path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(db_path), timeout=30)
    try:
        if conn.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("Underdog database integrity failed before migration")
        if not backup_path.exists():
            with sqlite3.connect(str(backup_path)) as backup:
                conn.backup(backup)
        conn.execute("BEGIN IMMEDIATE")
        conn.execute("""CREATE TABLE IF NOT EXISTS shaderlab_tool_pins (
            manifest_sha TEXT NOT NULL, name TEXT NOT NULL, version TEXT, kind TEXT NOT NULL,
            install_path TEXT, source_url TEXT, binary_sha256 TEXT,
            PRIMARY KEY(manifest_sha,name))""")
        conn.execute("""CREATE TABLE IF NOT EXISTS shaderlab_compiler_runs (
            receipt_sha TEXT NOT NULL, run_index INTEGER NOT NULL,
            shader TEXT NOT NULL, compiler TEXT NOT NULL, target TEXT NOT NULL,
            optimization TEXT, accepted INTEGER NOT NULL, bytecode_sha256 TEXT,
            bytecode_bytes INTEGER, diagnostics TEXT,
            PRIMARY KEY(receipt_sha,run_index))""")
        conn.execute("""CREATE TABLE IF NOT EXISTS shaderlab_gpu_runs (
            receipt_sha TEXT NOT NULL, run_index INTEGER NOT NULL,
            scene TEXT NOT NULL, backend TEXT NOT NULL, optimization TEXT NOT NULL,
            repeat_index INTEGER NOT NULL, accepted INTEGER NOT NULL,
            gpu_p50_ms REAL, image_sha256 TEXT, device_removed_reason TEXT,
            PRIMARY KEY(receipt_sha,run_index))""")
        conn.execute("""CREATE TABLE IF NOT EXISTS shaderlab_gpu_pairs (
            receipt_sha TEXT NOT NULL, pair_index INTEGER NOT NULL,
            scene TEXT NOT NULL, backend TEXT NOT NULL, repeat_index INTEGER NOT NULL,
            byte_identical INTEGER NOT NULL, o1_gpu_p50_ms REAL, o3_gpu_p50_ms REAL,
            PRIMARY KEY(receipt_sha,pair_index))""")
        conn.execute("""CREATE TABLE IF NOT EXISTS shaderlab_game_api_findings (
            receipt_sha TEXT NOT NULL, game TEXT NOT NULL,
            observed_api_imports_json TEXT NOT NULL, exe_sample_count INTEGER NOT NULL,
            sampled_shader_file_count INTEGER NOT NULL,
            truncated INTEGER NOT NULL, integration_status TEXT NOT NULL,
            PRIMARY KEY(receipt_sha,game))""")
        for t in tools["tools"]:
            conn.execute("INSERT OR IGNORE INTO shaderlab_tool_pins VALUES (?,?,?,?,?,?,?)",
                         (t_sha, t["name"], t.get("version") or t.get("version_pin"),
                          t["kind"], t.get("path"), t.get("url") or t.get("upstream") or t.get("source"),
                          t.get("sha256")))
        for i, r in enumerate(compiler["results"]):
            conn.execute("INSERT OR IGNORE INTO shaderlab_compiler_runs VALUES (?,?,?,?,?,?,?,?,?,?)",
                         (c_sha, i, r["shader"], r["compiler"], r["target"], r.get("optimization"),
                          int(r["ok"]), r.get("bytecode_sha256"), r.get("bytecode_bytes"),
                          r.get("diagnostics", "")))
        for i, r in enumerate(gpu["runs"]):
            conn.execute("INSERT OR IGNORE INTO shaderlab_gpu_runs VALUES (?,?,?,?,?,?,?,?,?,?)",
                         (g_sha, i, r["scene"], r["backend"], r["variant"], r["repeat"],
                          int(r["ok"]), float(r["gpu_p50_ms"]) if r.get("gpu_p50_ms") else None,
                          r.get("image_sha256"), r.get("device_removed_reason")))
        for i, r in enumerate(gpu["pairs"]):
            conn.execute("INSERT OR IGNORE INTO shaderlab_gpu_pairs VALUES (?,?,?,?,?,?,?,?)",
                         (g_sha, i, r["scene"], r["backend"], r["repeat"],
                          int(r["byte_identical"]), float(r["o1_p50_ms"]), float(r["o3_p50_ms"])))
        for r in games["games"]:
            conn.execute("INSERT OR IGNORE INTO shaderlab_game_api_findings VALUES (?,?,?,?,?,?,?)",
                         (a_sha, r["game"], json.dumps(r["observed_api_imports"]),
                          len(r["executables"]), len(r["shader_file_samples"]),
                          int(r["truncated"]), r["shaderlab_compatibility"]))
        conn.commit()
        verdict = conn.execute("PRAGMA integrity_check").fetchone()[0]
        if verdict != "ok":
            raise ValueError("Database integrity failed after migration: " + verdict)
        summary = {t: conn.execute("SELECT count(*) FROM " + t).fetchone()[0]
                   for t in ("shaderlab_tool_pins", "shaderlab_compiler_runs",
                             "shaderlab_gpu_runs", "shaderlab_gpu_pairs", "shaderlab_game_api_findings")}
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()
    pairs = gpu["pairs"]
    differences = [float(r["o3_p50_ms"]) - float(r["o1_p50_ms"]) for r in pairs]
    experience = {
        "schema": "underdog.shaderlab.experience_ledger.v1",
        "hypothesis": "FXC O3 yields a consistent performance improvement over the original O1",
        "result": "not_supported",
        "basis": "12 paired HD520 GPU P50 comparison observations over three scenes and two output backends",
        "o3_faster_pairs": sum(v < 0 for v in differences),
        "evaluated_pairs": len(differences),
        "median_o3_minus_o1_ms": statistics.median(differences),
        "image_parity_pairs": gpu["pairs_byte_identical"],
        "performance_promotion": False,
        "next_test": "Repeated interleaved A/B/A hardware trials with background-load control before any performance default",
        "receipts_sha256": {"compiler": c_sha, "gpu": g_sha, "game_audit": a_sha, "tools": t_sha},
        "db_rows": summary, "db_integrity": verdict,
        "authority": "local_execution_evidence_only; no Drive scope promotion",
    }
    ledger_path.parent.mkdir(parents=True, exist_ok=True)
    ledger_path.write_text(json.dumps(experience, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(experience, indent=2))


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    for item in ("database", "tools", "compiler", "gpu", "games", "backup", "ledger"):
        ap.add_argument("--" + item, type=Path, required=True)
    a = ap.parse_args()
    record(a.database, a.tools, a.compiler, a.gpu, a.games, a.backup, a.ledger)
