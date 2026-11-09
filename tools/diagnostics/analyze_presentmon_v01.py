#!/usr/bin/env python3
"""Reproducible PresentMon CSV summary; no guessed FPS or unsupported telemetry."""
import argparse,csv,hashlib,json,math,statistics
from pathlib import Path
from datetime import datetime,timezone
def percentile(items,q):
    if not items:return None
    v=sorted(items)
    idx=math.ceil(q*len(v))-1
    return v[min(max(idx,0),len(v)-1)]
def series(rows,key):
    result=[]
    for row in rows:
        try:
            val=float(row.get(key,"NA"))
            if math.isfinite(val) and val>=0:result.append(val)
        except (ValueError,TypeError):pass
    return result
def summary(values):
    return {"samples":len(values),"p50_ms":percentile(values,0.50),"p95_ms":percentile(values,0.95),
            "p99_ms":percentile(values,0.99),"mean_ms":statistics.mean(values) if values else None}
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--csv",required=True)
    p.add_argument("--process_id",required=True,type=int)
    p.add_argument("--scene",required=True)
    p.add_argument("--out",required=True)
    a=p.parse_args()
    path=Path(a.csv)
    with path.open(encoding="utf-8-sig",newline="") as f:rows=list(csv.DictReader(f))
    selected=[r for r in rows if r.get("Application","").lower()=="veloren-voxygen.exe" and r.get("ProcessID")==str(a.process_id)]
    if len(selected)<120:raise RuntimeError(f"Not enough frames {len(selected)}")
    delta=series(selected,"MsBetweenPresents")
    if len(delta)<120:raise RuntimeError("Less than 120 present intervals")
    arr={
        "schema":"underdog.presentmon.native_capture.v1",
        "captured_utc":datetime.now(timezone.utc).isoformat(),
        "scene":a.scene,"execution":"NATIVE_GAME_NO_UNDERDOG_ADAPTER",
        "presentmon_version":"2.6.0",
        "app":"veloren-voxygen.exe","pid":a.process_id,"hardware_id":"host02-intel-hd-520",
        "game_revision":"da6626e6","backend":"Dx12","driver":"22.20.16.4836",
        "csv_path":str(path),"csv_sha256":hashlib.sha256(path.read_bytes()).hexdigest(),
        "raw_rows":len(rows),"target_rows":len(selected),
        "present_interval":summary(delta),
        "gpu_time":summary(series(selected,"MsGPUTime")),
        "gpu_busy":summary(series(selected,"MsGPUBusy")),
        "cpu_busy":summary(series(selected,"MsCPUBusy")),
        "present_modes":sorted({r["PresentMode"] for r in selected if r.get("PresentMode")}),
        "notes":["Preliminary fixed-scene capture; cannot infer game-wide FPS", "Not paired with audited build yet",
                 "Metric validity depends on PresentMon ETW support for this OS and graphics driver"]}
    arr["aggregate_present_fps"]=1000.0/statistics.mean(delta) if delta and statistics.mean(delta)>0 else None
    out=Path(a.out);out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(arr,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"status":"ANALYZED","scene":a.scene,"frames":len(selected),
           "interval_p50_ms":arr["present_interval"]["p50_ms"],
           "interval_p95_ms":arr["present_interval"]["p95_ms"],
           "aggregate_present_fps":arr["aggregate_present_fps"],
           "gpu_p50_ms":arr["gpu_time"]["p50_ms"],"receipt":str(out)},indent=2))
if __name__=="__main__":main()
