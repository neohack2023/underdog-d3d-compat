#!/usr/bin/env python3
"""Bounded read-only Windows WER + Unreal RHI crash correlation for Intel HD 520.

No application launch, driver attach, process injection, or dump reading.
Never publishes raw log lines, user paths, kernel pointers or dump bytes.
"""
from datetime import datetime, timezone, timedelta
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT.parent.parent
LOCAL = PROJECT / "05_RESEARCH" / "INTEL_FORTNITE_GPU_FAULT_CORRELATION_V06.json"
PUBLIC = ROOT / "driver_intelligence" / "intel_hd520_fortnite_gpu_faults_v06.json"
NAMES = (
    "FortniteGame-backup-2026.11.09-02.22.02.log",
    "FortniteGame-backup-2026.11.09-02.36.32.log",
    "FortniteGame.log",
)
START, END = "2026-11-08 20:00:00", "2026-11-09 08:00:00"
LOG_TIMESTAMP = re.compile(r"\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]")
HRESULTS = {"DXGI_ERROR_DEVICE_HUNG": "0x887A0006",
            "DXGI_ERROR_DEVICE_REMOVED": "0x887A0005",
            "DXGI_ERROR_DRIVER_INTERNAL_ERROR": "0x887A0020"}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ps_events():
    script = r"""
$ErrorActionPreference='Stop'
$start=[datetime]::Parse('2026-11-08 20:00:00')
$end=[datetime]::Parse('2026-11-09 08:00:00')
$a=Get-WinEvent -FilterHashtable @{LogName='Application';Id=1001;StartTime=$start;EndTime=$end} -ErrorAction SilentlyContinue
$dred=@($a|Where-Object {$_.Message -match 'Event Name: D3DDRED2' -and $_.Message -match 'P5: FortniteClient-Win64-Shipping.exe'}|ForEach-Object {
  $s=$_.Message
  [pscustomobject]@{
    event_record_id=$_.RecordId
    time_local=$_.TimeCreated.ToString('o')
    vendor=([regex]::Match($s,'P1:\s*(\S+)')).Groups[1].Value
    driver_version=([regex]::Match($s,'P2:\s*(\S+)')).Groups[1].Value
    removed_hr=([regex]::Match($s,'P3:\s*(\S+)')).Groups[1].Value
    app=([regex]::Match($s,'P5:\s*(\S+)')).Groups[1].Value
  }
})
$disp=@(Get-WinEvent -FilterHashtable @{LogName='System';Id=4101;StartTime=$start;EndTime=$end} -ErrorAction SilentlyContinue|
Where-Object {$_.ProviderName -eq 'Display' -and $_.Message -match 'igfx'}|
ForEach-Object {[pscustomobject]@{event_record_id=$_.RecordId;time_local=$_.TimeCreated.ToString('o')}})
$kernel=@($a|Where-Object {$_.Message -match 'Event Name: LiveKernelEvent' -and $_.Message -match 'P1:\s*141'})
[pscustomobject]@{dred=$dred;display_recoveries=$disp;live_kernel_141_report_rows=$kernel.Count} |
ConvertTo-Json -Depth 6 -Compress
"""
    result = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive",
                             "-Command", script], capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=35)
    if result.returncode:
        raise RuntimeError("Read-only WER query failed; refuse partial classification")
    return json.loads(result.stdout)


def get_timestamp(lines, start_index, end_index):
    for i in range(start_index, min(end_index, len(lines))):
        m = LOG_TIMESTAMP.search(lines[i])
        if m:
            dt = datetime.strptime(m.group(1), "%Y.%m.%d-%H.%M.%S:%f")
            return dt.replace(tzinfo=timezone.utc).astimezone()
    return None


def parse_game(path, xml_path):
    raw=path.read_bytes()
    lines=raw.decode("utf-8-sig", errors="replace").splitlines()
    tags=[line for line in lines if "LogD3D12RHI: Error: GPU crash detected:" in line]
    assert len(tags)==1, (path.name,len(tags))
    event_index=lines.index(tags[0])
    gpu_time=get_timestamp(lines,event_index,event_index+2)
    assert gpu_time is not None
    crash_context=xml_path.read_text(encoding="utf-8-sig",errors="replace")
    assert "<CrashType>GPUCrash</CrashType>" in crash_context
    assert re.search(r"<RHI\.DRED>\s*false\s*</RHI\.DRED>",crash_context)
    first_resource=next((i for i,x in enumerate(lines) if "D3D12 CreatePlacedResource failed with params:" in x),None)
    first_pso=next((i for i,x in enumerate(lines) if "Failed to create pipeline state with combined hash" in x),None)
    first_map=next((i for i,x in enumerate(lines) if "Resource->Map(0, ReadRange" in x),None)
    descriptor=[]
    if first_resource is not None:
        for line in lines[first_resource+1:first_resource+8]:
            m=re.search(r"(Heap Type|Heap Flags|Resource Dimension|Resource Width|Resource Height|Format|Resource Flags):\s*(\d+)",line)
            if m:descriptor.append([m.group(1),int(m.group(2))])
    reason=next((text for text in ("DXGI_ERROR_DEVICE_HUNG","DXGI_ERROR_DRIVER_INTERNAL_ERROR")
                  if any("Device 0 Removed: "+text in x for x in lines)),None)
    assert reason is not None
    budgets={}
    for label in ("Local Budget","Local Used"):
        value=next((re.search(r"\b"+re.escape(label)+r":\s*(\d+(?:\.\d+)?)\s*MB",x)
                    for x in lines[event_index:] if label+":" in x),None)
        if value and value.group(1): budgets[label.lower().replace(" ","_")+"_mb"]=float(value.group(1))
    # Avoid exporting raw PSO descriptors, game content, crash XML, or absolute paths.
    return {
        "source_log_basename":path.name,"source_log_sha256":hashlib.sha256(raw).hexdigest(),
        "source_log_bytes":len(raw),
        "crash_context_sha256":sha(xml_path),
        "gpu_crash_time_local":gpu_time.isoformat(),
        "removed_reason":reason,"removed_hresult":HRESULTS[reason],
        "game_crash_line":event_index+1,
        "first_create_placed_resource_failure_line":first_resource+1 if first_resource is not None else None,
        "first_pso_creation_failure_line":first_pso+1 if first_pso is not None else None,
        "first_resource_map_failure_line":first_map+1 if first_map is not None else None,
        "created_resource_descriptor_observed":dict(descriptor),
        "failed_pso_creations":sum("Failed to create pipeline state with combined hash" in x for x in lines),
        "reported_video_memory":budgets,
        "unreal_dred_enabled":False,
        "intel_breadcrumbs_disabled":any("Intel Breadcrumbs is explicitly disabled" in x for x in lines),
        "d3d12_existing_heaps_unsupported_reported":any(
            "[GPUBreadCrumb] D3D12_FEATURE_EXISTING_HEAPS is not supported" in x for x in lines),
        "log_dred_has_breadcrumb_head":False if any("DRED: No breadcrumb head found" in x for x in lines) else None,
        "first_failure_is_root_cause":"NOT_PROVEN",
    }


def main():
    assert sys.platform=="win32", "Windows-only WER collector"
    localapp=os.environ.get("LOCALAPPDATA")
    if not localapp:raise EnvironmentError("LOCALAPPDATA unavailable")
    save=Path(localapp)/"FortniteGame"/"Saved"
    logs=save/"Logs"
    crashdirs=sorted((save/"Crashes").glob("UECC-Windows-*"),key=lambda x:x.stat().st_mtime)
    logfiles=[logs/n for n in NAMES]
    assert len(crashdirs)==3 and all(p.is_file() for p in logfiles)
    for path in logfiles:assert path.stat().st_size<15_000_000
    pairs=[]
    for logfile,crashdir in zip(logfiles,crashdirs):
        xml=crashdir/"CrashContext.runtime-xml"
        if not xml.is_file():raise FileNotFoundError(xml)
        # The crash XML is created within minutes of the game log; do not silently mispair.
        if abs(xml.stat().st_mtime-logfile.stat().st_mtime)>180:
            raise RuntimeError("Ambiguous crash/log pairing; inspect manually")
        pairs.append(parse_game(logfile,xml))
    report=ps_events()
    dred=sorted(report["dred"],key=lambda x:x["time_local"])
    disp=sorted(report["display_recoveries"],key=lambda x:x["time_local"])
    assert len(dred)==2 and len(disp)==10
    assert all(x["vendor"]=="VEN_8086" and x["driver_version"]=="22.20.16.4836"
               and x["removed_hr"].lower()=="0x887a0006" for x in dred)
    assert [x["removed_reason"] for x in pairs]==[
        "DXGI_ERROR_DEVICE_HUNG","DXGI_ERROR_DEVICE_HUNG","DXGI_ERROR_DRIVER_INTERNAL_ERROR"]
    for crash in pairs[:2]:
        when=datetime.fromisoformat(crash["gpu_crash_time_local"])
        assert any(abs((when-datetime.fromisoformat(x["time_local"])).total_seconds())<7 for x in dred)
    third=datetime.fromisoformat(pairs[2]["gpu_crash_time_local"])
    assert any(abs((third-datetime.fromisoformat(x["time_local"])).total_seconds())<5 for x in disp)
    assert all(x["intel_breadcrumbs_disabled"] and x["d3d12_existing_heaps_unsupported_reported"]
               and x["unreal_dred_enabled"] is False for x in pairs)
    local={
      "schema":"aios.underdog.intel_hd520.fortnite_device_hang_research.v6",
      "observed_utc":datetime.now(timezone.utc).isoformat(),
      "time_window_local":[START,END],
      "hardware":"Intel HD Graphics 520 (8086:1916)",
      "driver":"22.20.16.4836",
      "reporting_events_dred2":dred,
      "system_display_recoveries_4101":disp,
      "live_kernel_event141_report_rows":report["live_kernel_141_report_rows"],
      "game_incidents":pairs,
      "root_cause":"UNRESOLVED",
      "driver_callback_rva":"UNKNOWN",
      "limits":["WER D3DDRED2 and UE logs correlate Fortnite with GPU device errors",
                "Resource and PSO failures may be consequences after device removal",
                "DRED explicitly off; GPU breadcrumbs/page faults unavailable",
                "Repeated WER LiveKernelEvent141 rows need not represent unique hangs",
                "No Fortnite binary accessed; no process injection or driver modification"]
    }
    LOCAL.parent.mkdir(parents=True,exist_ok=True)
    LOCAL.write_text(json.dumps(local,indent=2)+"\n",encoding="utf-8")
    public={
      "schema":"underdog.intel_gen9.fortnite_gpu_faults.v6",
      "hardware_id":"host02-intel-hd-520",
      "gpu":"Intel HD Graphics 520","driver":"22.20.16.4836",
      "event_date_local":"2026-11-08 to 2026-11-09",
      "dred2_fortnite_event_count":len(dred),
      "display_driver_recoveries_4101_count":len(disp),
      "game_incidents":[{
        "time_local":x["gpu_crash_time_local"],
        "device_removed_reason":x["removed_reason"],
        "d3d12_failure_indicators":{
          "create_placed_resource":x["first_create_placed_resource_failure_line"] is not None,
          "pipeline_creation":x["first_pso_creation_failure_line"] is not None,
          "resource_map":x["first_resource_map_failure_line"] is not None,
        },
        "failed_pso_count":x["failed_pso_creations"],
        "reported_memory_mb":x["reported_video_memory"],
        "unreal_dred_enabled":x["unreal_dred_enabled"],
        "breadcrumbs_available":False,
      } for x in pairs],
      "evidence_method":"Read-only Windows WER ID1001/Display ID4101 plus 3 Unreal game logs and crash contexts on Host02",
      "local_evidence_file":"05_RESEARCH/INTEL_FORTNITE_GPU_FAULT_CORRELATION_V06.json",
      "root_cause":"UNKNOWN",
      "specific_intel_callback":"UNRESOLVED",
      "limitation":"Device-hung or driver-internal-error classification does not prove which Intel driver function triggered it.",
      "raw_log_data_distributed":False,
      "proprietary_driver_or_game_binaries_distributed":False,
    }
    PUBLIC.write_text(json.dumps(public,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"status":"FORTNITE_DRED_WER_CORRELATION_VERIFIED",
                      "dred2_event_count":len(dred),"display_recoveries":len(disp),
                      "game_incidents":len(pairs),"dred_enabled":False,
                      "root_cause":"UNRESOLVED","public_map":str(PUBLIC),
                      "local_research":str(LOCAL)},indent=2))


if __name__=="__main__":main()
