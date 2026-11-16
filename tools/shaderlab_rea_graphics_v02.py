#!/usr/bin/env python3
"""Merge REA PE evidence and driver diff into graphics feature hypotheses, not claims."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path

API_KEYWORDS = ("direct3d", "d3dx", "d3d", "ddraw", "shader", "texture", "gl", "vulkan")
ABI = {"d3d9.dll":"D3D9", "d3d8.dll":"D3D8", "ddraw.dll":"DirectDraw",
       "d3dx9_43.dll":"D3DX9 helper library", "opengl32.dll":"OpenGL"}
DRIVERS = ("igd12umd64.dll","igd10iumd64.dll","ig9icd64.dll","igc64.dll","igdumdim32.dll")

def read(path: Path):
    contents = path.read_bytes()
    return json.loads(contents.decode("utf-8-sig")), hashlib.sha256(contents).hexdigest()

def game_report(name, path):
    evidence, digest = read(path)
    if evidence.get("operation") != "inspect_pe_imports":
        raise ValueError(f"Invalid REA PE-import evidence: {path}")
    observed = evidence["normalized_result"]
    matches = []
    for kind, table in (("import", "imports"), ("delay_import", "delay_imports")):
        for item in observed.get(table, []):
            dll = item.get("name", "").lower()
            if dll not in ABI:
                continue
            names = [fun["name"] for fun in item.get("functions", []) if fun.get("name")]
            matches.append({"module":dll,"kind":kind,"api":ABI[dll],"functions":names})
    hints=[]
    for item in matches:
        for fun in item["functions"]:
            if fun in ("Direct3DCreate8","Direct3DCreate9"):
                hints.append({"feature":"API device creation entry point","source":item["module"]+"!"+fun})
            if fun=="D3DXGetShaderConstantTable":
                hints.append({"feature":"D3D9 shader constant-table helper","source":item["module"]+"!"+fun})
    return {"game":name,"rea_evidence_sha256":digest,
            "exe_sha256":evidence["subject"]["digest"]["sha256"],
            "architecture":observed.get("architecture"),"graphics_imports":matches,
            "feature_entrypoint_hints":hints,
            "unknowns":["vtable device method usage", "actual shader program content",
                        "shader version", "runtime selected renderer", "rendered output"],
            "status":"read_only_static_import_evidence"}

def driver_report(name,oldpath,newpath):
    old,old_sha=read(oldpath);new,new_sha=read(newpath)
    if old.get("operation")!="inspect_native_exports" or new.get("operation")!="inspect_native_exports":
        raise ValueError("Not a pair of REA export reports")
    x=old["normalized_result"].get("exports",[])
    y=new["normalized_result"].get("exports",[])
    names=lambda records: {str(e.get("name") or ("ordinal:"+str(e.get("ordinal")))) for e in records}
    a,b=names(x),names(y)
    return {"module":name,"rea_older_sha256":old_sha,"rea_newer_sha256":new_sha,
            "older_exports":len(x),"newer_exports":len(y),
            "removed_exports":sorted(a-b),"added_exports":sorted(b-a),
            "feature_inference":"export identity only; internal semantics and capabilities unknown"}

def build(outdir:Path):
    games=[game_report("MGRR",outdir/"mgrr_rea_pe_imports.json"),
           game_report("UT2004",outdir/"ut2004_rea_pe_imports.json")]
    modules=[driver_report(n,outdir/(n+".exports.json"),
                           outdir/(n+".5174.exports.json")) for n in DRIVERS]
    return {"schema":"underdog.shaderlab.graphics_abi_rea.v02",
            "tool":"REA 6.9.0 offline PE imports/exports",
            "game_evidence":games,"driver_evidence":modules,
            "verification":"native PE metadata, not GPU execution",
            "driver_pair":"21.20.16.4534 vs 21.20.16.5174",
            "next_rea_targets":[
                "MGRR: Direct3DCreate9 import thunk xrefs, associated IDirect3DDevice9 vtable calls",
                "MGRR: D3DXGetShaderConstantTable import thunk and resource provenance",
                "UT2004: Direct3DCreate8 import thunk and fixed-function state handling",
                "Intel: compare OpenAdapter12 function table and compiler-facing modules by RVA, not virtual address",
                "IGC: resolve 12 removed Vulkan-flavored exports before inferring capabilities"
            ]}

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--evidence",type=Path,required=True)
    ap.add_argument("--output",type=Path,required=True)
    a=ap.parse_args()
    data=build(a.evidence)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(data,indent=2)+"\n",encoding="utf-8")
    print("REPORT",a.output)
    for item in data["game_evidence"]:
        print(item["game"],[(m["module"],len(m["functions"])) for m in item["graphics_imports"]])
    for item in data["driver_evidence"]:
        print(item["module"],"exports",item["older_exports"],"->",item["newer_exports"])

if __name__ == "__main__":
    main()
