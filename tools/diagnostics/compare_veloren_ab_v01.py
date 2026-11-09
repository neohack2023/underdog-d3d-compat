#!/usr/bin/env python3
"""Strict native/source-built/opt-in audit PresentMon comparison. Refuses incomplete runs."""
import argparse,json,pathlib,hashlib,math
def load(path):
    p=pathlib.Path(path)
    if not p.is_file():raise FileNotFoundError("Comparison blocked: missing "+str(p))
    return json.loads(p.read_text(encoding="utf-8"))
def number(obj,field):
    val=obj[field]
    if isinstance(val,(int,float)) and math.isfinite(val):return val
    raise ValueError("Missing/invalid metric "+field)
def validate(r):
    assert r["schema"]=="underdog.presentmon.native_capture.v1"
    assert r["target_rows"]>=120 and r["present_interval"]["samples"]>=120
    assert r["backend"]=="Dx12" and r["game_revision"]=="da6626e6"
    assert r["hardware_id"]=="host02-intel-hd-520" and r["driver"]=="22.20.16.4836"
    return r
def main():
    p=argparse.ArgumentParser(description="Compare true A/B/C, refusing native-only or missing audit controls")
    p.add_argument("--native",required=True)
    p.add_argument("--source_off",required=True)
    p.add_argument("--source_on",required=True)
    p.add_argument("--out",required=True)
    a=p.parse_args()
    native=validate(load(a.native))
    off=validate(load(a.source_off))
    on=validate(load(a.source_on))
    assert native["execution"]=="NATIVE_GAME_NO_UNDERDOG_ADAPTER"
    assert off["execution"]=="SOURCE_BUILD_AUDIT_OFF"
    assert on["execution"]=="SOURCE_BUILD_AUDIT_ON"
    assert off["binary_sha256"]==on["binary_sha256"] and off["binary_sha256"]!=native.get("binary_sha256")
    assert on.get("audit_patch_sha256") and off.get("audit_patch_sha256")==on.get("audit_patch_sha256")
    assert on.get("audit_log_sha256") and pathlib.Path(on["audit_log_path"]).is_file()
    assert all(x["scene"]==native["scene"] for x in [off,on])
    assert all(x["backend"]==native["backend"] and x["hardware_id"]==native["hardware_id"] and x["driver"]==native["driver"] for x in [off,on])
    assert on.get("image_parity_pass") is True
    f=lambda x:number(x,"aggregate_present_fps")
    pf=lambda x:x["present_interval"]["p95_ms"]
    result={"schema":"underdog.veloren.native_audit_comparison.v1",
      "status":"PAIRED_MEASUREMENT_ONLY_NOT_COMPATIBILITY_PROMOTION",
      "hardware_id":native["hardware_id"],"scene":native["scene"],
      "native_fps":f(native),"source_off_fps":f(off),"source_on_fps":f(on),
      "audit_on_vs_off_fps_percent":100*(f(on)/f(off)-1),
      "audit_on_vs_off_p95_ms":pf(on)-pf(off),
      "controls":"A official native, B source-built audit OFF, C identical binary audit ON. No policy-enforce D included.",
      "causality_warning":"Require multiple trials and controlled world/time/route before conclusions."}
    pathlib.Path(a.out).write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result,indent=2))
if __name__=="__main__":main()
