#!/usr/bin/env python3
"""Import only metadata and evidence hashes; no proprietary shader or driver binary."""
import argparse
import hashlib
import json
import sqlite3
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def store(database:Path, abi_path:Path, pe_path:Path, backup:Path):
    abi=json.loads(abi_path.read_text(encoding="utf-8"))
    pe=json.loads(pe_path.read_text(encoding="utf-8"))
    assert abi["schema"]=="underdog.shaderlab.graphics_abi_rea.v02"
    assert pe["schema"]=="underdog.shaderlab.driver_version_comparison.v02"
    if not database.is_file():
        raise FileNotFoundError(database)
    with sqlite3.connect(database,timeout=30) as conn:
        if conn.execute("PRAGMA integrity_check").fetchone()[0]!="ok":
            raise RuntimeError("DB integrity preflight failed")
        if not backup.exists():
            backup.parent.mkdir(parents=True,exist_ok=True)
            with sqlite3.connect(backup) as b:
                conn.backup(b)
        conn.execute("""CREATE TABLE IF NOT EXISTS shaderlab_v02_game_rea_graphics (
            receipt_sha256 TEXT NOT NULL, game TEXT NOT NULL,
            game_exe_sha256 TEXT NOT NULL, architecture TEXT,
            graphics_imports_json TEXT NOT NULL, feature_hints_json TEXT NOT NULL,
            semantics_status TEXT NOT NULL,
            PRIMARY KEY(receipt_sha256,game))""")
        conn.execute("""CREATE TABLE IF NOT EXISTS shaderlab_v02_driver_abi_changes (
            receipt_sha256 TEXT NOT NULL, module TEXT NOT NULL,
            old_module_sha256 TEXT NOT NULL, new_module_sha256 TEXT NOT NULL,
            old_exports INTEGER NOT NULL, new_exports INTEGER NOT NULL,
            removed_export_names_json TEXT NOT NULL, added_export_names_json TEXT NOT NULL,
            inference_state TEXT NOT NULL,
            PRIMARY KEY(receipt_sha256,module))""")
        a_sha=digest(abi_path)
        for g in abi["game_evidence"]:
            conn.execute("INSERT OR IGNORE INTO shaderlab_v02_game_rea_graphics VALUES (?,?,?,?,?,?,?)",
                         (a_sha,g["game"],g["exe_sha256"],g["architecture"],
                          json.dumps(g["graphics_imports"]),json.dumps(g["feature_entrypoint_hints"]),
                          g["status"]))
        module_by_name={row["name"]:row for row in pe["modules"]}
        for m in abi["driver_evidence"]:
            pm=module_by_name[m["module"]]
            if pm["old"]["sha256"]==pm["new"]["sha256"]:
                raise ValueError("Unexpected identical driver module digest: "+m["module"])
            conn.execute("INSERT OR IGNORE INTO shaderlab_v02_driver_abi_changes VALUES (?,?,?,?,?,?,?,?,?)",
                         (a_sha,m["module"],pm["old"]["sha256"],pm["new"]["sha256"],
                          m["older_exports"],m["newer_exports"],
                          json.dumps(m["removed_exports"]),json.dumps(m["added_exports"]),
                          "static_abi_delta_only"))
        conn.commit()
        integrity=conn.execute("PRAGMA integrity_check").fetchone()[0]
        counts={name:conn.execute("SELECT COUNT(*) FROM "+name).fetchone()[0] for name in
                ("shaderlab_v02_game_rea_graphics","shaderlab_v02_driver_abi_changes")}
    return {"integrity":integrity,"counts":counts,"abi_evidence_sha256":digest(abi_path),
            "pe_comparison_sha256":digest(pe_path),
            "disposition":"offline static evidence; no driver or game runtime compatibility implied"}

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    for flag in ("database","abi","pe","backup","output"):
        ap.add_argument("--"+flag,type=Path,required=True)
    a=ap.parse_args()
    result=store(a.database,a.abi,a.pe,a.backup)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result,indent=2))
