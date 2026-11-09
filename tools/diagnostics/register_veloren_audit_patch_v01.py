#!/usr/bin/env python3
"""Verify isolated GPL Veloren audit patch and register BLOCKED build state."""
import hashlib,json,pathlib,sqlite3,subprocess,tempfile,shutil
from datetime import datetime,timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
BASE=ROOT/"07_EXECUTION"/"VELOREN_POLICY_AUDIT_V01"
REC=BASE/"audit_patch_receipt_v01.json"
PATCH=BASE/"veloren_da6626e6_underdog_audit_v01.patch"
DB=ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
LEDGER=ROOT/"04_KNOWLEDGE"/"DX12_HD520_FEATURE_EXPERIENCE_V01.jsonl"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
r=json.loads(REC.read_text(encoding="utf-8"))
assert PATCH.is_file() and sha(PATCH)==r["patch_sha256"]
assert r["git_ref"]=="da6626e6"
assert r["license"].startswith("GPL")
for key,digest in r["upstream_file_sha256"].items():
    p=BASE/{"voxygen/src/render/renderer/mod.rs":"renderer_mod.rs", "voxygen/src/render/texture.rs":"texture.rs", "voxygen/src/render/renderer/binding.rs":"binding.rs"}[key]
    assert p.exists() and sha(p)==digest,(p,digest)
# Recreate temporary exact-source checkout each time; never rely on stale scratch.
with tempfile.TemporaryDirectory(prefix="veloren_patch_check_",dir=BASE) as ephemeral:
    stage=pathlib.Path(ephemeral)
    names={"voxygen/src/render/renderer/mod.rs":"renderer_mod.rs",
           "voxygen/src/render/texture.rs":"texture.rs",
           "voxygen/src/render/renderer/binding.rs":"binding.rs"}
    for rel,source in names.items():
        dest=stage/rel
        dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(BASE/source,dest)
    for args in (
        ["git","apply","--check",str(PATCH)],
        ["git","apply",str(PATCH)],
        ["git","apply","--reverse","--check",str(PATCH)],
    ):
        check=subprocess.run(args,cwd=str(stage),stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        if check.returncode:
            raise RuntimeError("Patch validation failed: "+check.stderr)
r["status"]="PATCH_APPLIES_TO_PINNED_SOURCE_NOT_COMPILED"
r["patch_check_status"]="GIT_APPLY_REVERSE_CHECK_PASS"
r["compiled_game_executable"]=None
r["runtime_adaptor_tested"]=False
r["updated_utc"]=datetime.now(timezone.utc).isoformat()
REC.write_text(json.dumps(r,indent=2)+"\n",encoding="utf-8")
with sqlite3.connect(DB) as conn:
    assert conn.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    conn.execute("""INSERT OR REPLACE INTO experiments
      (experiment_id,name,executed_on_hardware,results_path,description,status,origin_database,imported_utc)
      VALUES(?,?,?,?,?,?,?,?)""",(
      "VELOREN-UNDERDOG-WGPU-AUDIT-PATCH-V01","GPL isolated wgpu resource/atlas audit patch",0,
      str(REC),"Pinned Veloren source static apply verified, no Rust toolchain, no game executable",
      "PATCH_APPLY_VERIFIED_BUILD_NOT_AVAILABLE",str(DB),r["updated_utc"]))
    aid=hashlib.sha256(str(PATCH).encode()).hexdigest()[:24]
    conn.execute("""INSERT OR IGNORE INTO artifacts
      (artifact_id,role,absolute_path,extension,bytes_on_source_disk,present,origin_workspace,indexed_utc)
      VALUES(?,?,?,?,?,?,?,?)""",(aid,"GPL_CONSUMER_PATCH",str(PATCH),".patch",PATCH.stat().st_size,1,str(BASE),r["updated_utc"]))
    assert conn.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    conn.commit()
ev={"id":"VELOREN-UNDERDOG-WGPU-AUDIT-PATCH-V01","utc":r["updated_utc"],
    "claim":"GPL source-level audit patch statically applies to pinned Veloren source",
    "outcome":"PATCH_READY_NOT_COMPILED_NOT_EXECUTED","evidence":str(REC),
    "blocking_dependency":"Rust/Cargo and isolated source build absent on Host02"}
prior=[json.loads(x) for x in LEDGER.read_text(encoding="utf-8").splitlines() if x.strip()]
if not any(x.get("id")==ev["id"] for x in prior):
    with LEDGER.open("a",encoding="utf-8") as f:f.write(json.dumps(ev)+"\n")
print(json.dumps({"status":r["status"],"patch_sha256":r["patch_sha256"],
                  "toolchain_blocker":"NO_RUST_OR_CARGO",
                  "receipt":str(REC)},indent=2))
