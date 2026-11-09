#!/usr/bin/env python3
"""Register verified signed PresentMon CLI locally; does not claim Drive authority."""
import json,hashlib,pathlib,sqlite3
from datetime import datetime,timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
DB=ROOT/"02_DATABASE"/"UNDERDOG_D3D_COMPAT_V01.sqlite"
OUT=ROOT/"00_MANIFESTS"/"PRESENTMON_V260_TOOL_REGISTRATION_V01.json"
EXE=pathlib.Path(r"C:\AIOS\01_TOOLS\PRESENTMON_V260\PresentMon-2.6.0-x64.exe")
EXPECTED="b2a706bc6ad475749e3b7e3409263aa1e6906d45bdcf993f6dbc0f660188f1af"
if not EXE.is_file() or EXE.stat().st_size!=980320:raise RuntimeError("PresentMon binary missing or length mismatch")
sha=hashlib.sha256(EXE.read_bytes()).hexdigest()
assert sha==EXPECTED
source="https://github.com/GameTechDev/PresentMon/releases/download/v2.6.0/PresentMon-2.6.0-x64.exe"
world=ROOT/"06_EVIDENCE"/"VELOREN_NATIVE_DX12_PRESENTMON_WORLD_V01.json"
assert world.is_file()
when=datetime.now(timezone.utc).isoformat()
with sqlite3.connect(DB) as conn:
    conn.execute("PRAGMA foreign_keys=ON")
    assert conn.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    compid="presentmon-v260-host02"
    conn.execute("INSERT OR REPLACE INTO components VALUES(?,?,?,?,?,?)",
       (compid,"PresentMon 2.6.0 signed standalone CLI",str(EXE),"SIGNED_EXTERNAL_CLI",
        "LOCAL_RUNTIME_VERIFIED",f"SHA256 {sha}; vendor Authenticode=Valid on Host02; official {source}"))
    conn.execute("INSERT OR REPLACE INTO capabilities VALUES(?,?,?,?,?,?,?)",
       ("presentmon-frame-pacing","Native D3D12 ETW presentation timing",
        "PresentMon 2.6.0 console CSV","VERIFIED_354_UI_209_WORLD_PRESENTS",
        compid,str(world),"GPU time depends on ETW/driver; menu and world differ; not an Underdog A/B test"))
    artifactid=hashlib.sha256(str(EXE).encode()).hexdigest()[:24]
    conn.execute("""INSERT OR IGNORE INTO artifacts
       (artifact_id,role,absolute_path,extension,bytes_on_source_disk,present,origin_workspace,indexed_utc)
       VALUES(?,?,?,?,?,?,?,?)""",(artifactid,"SIGNED_PERFORMANCE_TOOL",str(EXE),".exe",EXE.stat().st_size,1,str(EXE.parent),when))
    assert conn.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
    conn.commit()
manifest={"schema":"aios.underdog.presentmon_tool_registration.v1","captured_utc":when,
 "tool":"Intel PresentMon Console 2.6.0","version":"2.6.0","path":str(EXE),
 "sha256":sha,"bytes":EXE.stat().st_size,"authenticode_observed":"Valid",
 "origin":source,"license":"Upstream distribution license applies",
 "status":"REGISTERED_IN_LOCAL_UNDERDOG_DATABASE_NOT_DRIVE_SCOPE_CANON",
 "validated_runs":["VELOREN-NATIVE-UI-PM260-V01","VELOREN-NATIVE-WORLD-PM260-V01"],
 "notes":"No MSI installation. Performance raw CSVs preserved separately. Avoid duplicate tools."}
OUT.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"status":"LOCAL_TOOL_REGISTERED","sha256":sha,"manifest":str(OUT)}))
