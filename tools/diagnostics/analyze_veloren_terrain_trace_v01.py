#!/usr/bin/env python3
"""Analyze opt-in GPL Veloren terrain lifecycle logs without interpreting guesses as root cause.
Parses only messages tagged underdog_terrain_trace.
"""
import argparse,collections,hashlib,json,pathlib,re
from datetime import datetime,timezone
ANSI=re.compile(r"\x1b\[[0-9;]*m")
FIELD=re.compile(r'(\w+)=(?:"([^"]*)"|(-?[0-9]+)|([^\s]+))')
STAGES={"REMOVED","QUEUED","NEIGHBOR_WAIT","SAMPLE_WAIT","WORKER_START",
        "WORKER_RECV","OLDER_RESULT_ADMITTED","ATLAS_RESET","UPLOAD_BEGIN",
        "MODEL_STORED","REJECTED_TICK","REJECTED_REMOVED","VISIBLE","CULLED",
        "FOCUS_DRAW_GATE"}
def parse(lines):
    events=[]
    for line_no,line in enumerate(lines,1):
        if "underdog_terrain_trace" not in line:continue
        line=ANSI.sub("",line)
        fields={name:a or b or c for name,a,b,c in FIELD.findall(line)}
        if "stage" not in fields or "x" not in fields or "y" not in fields:
            continue
        if fields["stage"] not in STAGES:continue
        try:
            x,y=int(fields["x"]),int(fields["y"])
            tick=int(fields.get("tick","0"))
        except ValueError:continue
        events.append({"line":line_no,"stage":fields["stage"],
                       "pos":[x,y],"tick":tick,"detail":fields.get("detail",""),
                       "source":line[:300]})
    return events
def analyze(events, target=None, radius=0):
    kept=[ev for ev in events if target is None or
          (abs(ev["pos"][0]-target[0])<=radius and abs(ev["pos"][1]-target[1])<=radius)]
    bychunk=collections.defaultdict(list)
    for ev in kept:bychunk[tuple(ev["pos"])].append(ev)
    rows=[]
    for pos,sequence in sorted(bychunk.items(),key=lambda x:(-len(x[1]),x[0])):
        names=collections.Counter(ev["stage"] for ev in sequence)
        draw=collections.Counter(ev["detail"] for ev in sequence if ev["stage"]=="FOCUS_DRAW_GATE")
        checks=[]
        if names["NEIGHBOR_WAIT"] or names["SAMPLE_WAIT"]:
            checks.append("MISSING_3X3_NEIGHBOR_OR_TERRAIN_SAMPLE")
        if names["OLDER_RESULT_ADMITTED"]:checks.append("OLDER_WORKER_RESULT_ADMITTED")
        if names["ATLAS_RESET"]:checks.append("ATLAS_ALLOCATION_RESET")
        if names["REJECTED_TICK"] or names["REJECTED_REMOVED"]:checks.append("WORKER_RESULT_REJECTED")
        if draw["chunk_not_in_renderer"]:checks.append("PLAYER_FOCUS_CHUNK_ABSENT_FROM_RENDER_MAP")
        if draw["no_opaque_gpu_model"]:checks.append("PLAYER_FOCUS_CHUNK_WITHOUT_OPAQUE_MODEL")
        if draw["model_not_visible"]:checks.append("PLAYER_FOCUS_CHUNK_NOT_VISIBLE")
        if names["MODEL_STORED"] and draw["chunk_not_in_renderer"]:
            checks.append("STORED_AND_MISSING_SAME_CHUNK_AT_DIFFERENT_SAMPLED_TIMES_CHECK_SEQUENCE")
        rows.append({"chunk":list(pos),"events":len(sequence),
                     "first_line":sequence[0]["line"],"last_line":sequence[-1]["line"],
                     "stages":dict(names),"focus_draw_states":dict(draw),
                     "diagnostic_flags":checks})
    return {"total_parsed_events":len(events),"selected_events":len(kept),
            "chunk_count":len(rows),"stage_totals":dict(collections.Counter(e["stage"] for e in kept)),
            "chunks":rows[:500],
            "classification":"LIFECYCLE_COUNTERS_ONLY_NO_CONFIRMED_GPU_PRESENT",
            "interpretation_limits":[
                "No concrete root cause without a captured bad frame and matching chunk coordinates",
                "MODEL_STORED proves CPU-side model creation, not GPU render/present",
                "A focus chunk with no opaque model may be legitimate empty/air terrain",
                "Visibility changes can be normal; compare camera position",
                "Event limit 8192 may truncate a long run and is not a persistent ring buffer"]}
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--log")
    p.add_argument("--out")
    p.add_argument("--chunk",help="x,y coordinate")
    p.add_argument("--radius",type=int,default=0)
    p.add_argument("--self-test",action="store_true")
    args=p.parse_args()
    if args.self_test:
        sample=[
            '2026-11-09T22:00:00Z INFO underdog_terrain_trace: terrain lifecycle stage="NEIGHBOR_WAIT" x=12 y=-3 tick=100 detail="not_all_3x3_chunks_available"',
            '2026-11-09T22:00:01Z INFO underdog_terrain_trace: terrain lifecycle stage="QUEUED" x=12 y=-3 tick=101 detail="neighbors_present"',
            '2026-11-09T22:00:02Z INFO underdog_terrain_trace: terrain lifecycle stage="WORKER_START" x=12 y=-3 tick=101 detail="meshing_job_spawn"',
            '2026-11-09T22:00:03Z INFO underdog_terrain_trace: terrain lifecycle stage="OLDER_RESULT_ADMITTED" x=12 y=-3 tick=100 detail="pending_todo_has_newer_tick"',
            '2026-11-09T22:00:04Z INFO underdog_terrain_trace: terrain lifecycle stage="MODEL_STORED" x=12 y=-3 tick=101 detail="model_and_atlas_created"',
            '2026-11-09T22:00:05Z INFO underdog_terrain_trace: terrain lifecycle stage="FOCUS_DRAW_GATE" x=12 y=-3 tick=0 detail="drawable_model_present"']
        events=parse(sample)
        report=analyze(events,target=(12,-3))
        assert len(events)==6 and report["selected_events"]==6
        assert report["chunk_count"]==1
        assert "MISSING_3X3_NEIGHBOR_OR_TERRAIN_SAMPLE" in report["chunks"][0]["diagnostic_flags"]
        assert "OLDER_WORKER_RESULT_ADMITTED" in report["chunks"][0]["diagnostic_flags"]
        print(json.dumps({"status":"PARSER_SELF_TEST_PASS","synthetic_events":6,
                          "actual_gpu_events":0,"demo_only":True}))
        return
    if not args.log or not args.out: p.error("Provide --log and --out, or --self-test")
    path=pathlib.Path(args.log)
    if not path.is_file():raise FileNotFoundError(path)
    raw=path.read_bytes()
    lines=raw.decode("utf-8-sig",errors="replace").splitlines()
    target=None
    if args.chunk:
        target=tuple(int(v) for v in args.chunk.split(","))
        if len(target)!=2:raise ValueError("Chunk requires x,y")
    if args.radius<0 or args.radius>128:raise ValueError("Radius out of bounds")
    events=parse(lines)
    result=analyze(events,target=target,radius=args.radius)
    result.update({"schema":"aios.underdog.veloren.terrain_events.v1",
                   "captured_utc":datetime.now(timezone.utc).isoformat(),
                   "raw_log_path":str(path),"raw_log_sha256":hashlib.sha256(raw).hexdigest(),
                   "target_chunk":target,"radius":args.radius,
                   "status":"PARSED_LIVE_EVENTS" if events else "NO_TERRAIN_TRACE_EVENTS_FOUND"})
    dest=pathlib.Path(args.out)
    dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"status":result["status"],"events":len(events),
                      "selected":result["selected_events"],"chunks":result["chunk_count"],
                      "report":str(dest)}))
if __name__=="__main__":main()
