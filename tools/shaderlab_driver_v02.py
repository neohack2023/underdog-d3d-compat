"""Static driver-version comparator for official Intel archives; never loads a DLL."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

NAMES = ("igd12umd64.dll", "igd10iumd64.dll", "ig9icd64.dll", "igc64.dll", "igdumdim32.dll")
EXPECTED_ZIP_SHA = "6a1d64d48e52913f8c42587900dec47d050daa4cde7516f9dc6abadd6e6404fb"

def sha(path: Path) -> str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda:f.read(1048576), b""):
            h.update(block)
    return h.hexdigest()

def unpack(zip_path:Path,out:Path):
    if sha(zip_path)!=EXPECTED_ZIP_SHA:
        raise ValueError("Official Intel archive SHA256 did not match publisher")
    out.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(zip_path) as z:
        rows = []
        for name in NAMES:
            matches=[item for item in z.infolist() if item.filename=="Graphics/"+name]
            if len(matches)!=1 or matches[0].file_size>110000000:
                raise ValueError("Missing, duplicated, or unexpectedly large Intel library: "+name)
            target=out/name
            if not target.is_file() or target.stat().st_size!=matches[0].file_size:
                with z.open(matches[0]) as source, target.open("wb") as dest:
                    for block in iter(lambda:source.read(1048576), b""):
                        dest.write(block)
            rows.append({"name":name,"path":str(target),"size":target.stat().st_size,"sha256":sha(target)})
        return rows

def compare(baseline:Path,candidate:Path):
    import pefile
    rows=[]
    for name in NAMES:
        a=baseline/name;b=candidate/name
        if not a.is_file() or not b.is_file():
            rows.append({"name":name,"status":"missing"})
            continue
        objects=[]
        for p in (a,b):
            pe=pefile.PE(str(p),fast_load=True)
            try:
                pe.parse_data_directories(directories=[
                    pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"],
                    pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
                exports=sorted({(item.name or b"#"+str(item.ordinal).encode()).decode("utf-8","replace")
                                for item in getattr(getattr(pe,"DIRECTORY_ENTRY_EXPORT",None),"symbols",[])})
                imports=sorted({item.dll.decode("utf-8","replace").lower()
                                for item in getattr(pe,"DIRECTORY_ENTRY_IMPORT",[])})
                objects.append({"sha256":sha(p),"bytes":p.stat().st_size,
                                "timestamp":pe.FILE_HEADER.TimeDateStamp,
                                "machine":hex(pe.FILE_HEADER.Machine),
                                "export_names":exports,"import_dlls":imports})
            finally:
                pe.close()
        a1,b1=objects
        rows.append({"name":name,"old":a1,"new":b1,
                     "export_names_added":sorted(set(b1["export_names"])-set(a1["export_names"])),
                     "export_names_removed":sorted(set(a1["export_names"])-set(b1["export_names"])),
                     "imports_added":sorted(set(b1["import_dlls"])-set(a1["import_dlls"])),
                     "imports_removed":sorted(set(a1["import_dlls"])-set(b1["import_dlls"]))})
    return rows

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--zip",type=Path,required=True)
    ap.add_argument("--candidate",type=Path,required=True)
    ap.add_argument("--baseline",type=Path,required=True)
    ap.add_argument("--output",type=Path,required=True)
    args=ap.parse_args()
    extracted=unpack(args.zip,args.candidate)
    rows=compare(args.baseline,args.candidate)
    report={"schema":"underdog.shaderlab.driver_version_comparison.v02",
            "source":"official Intel 15.45.34.5174 Windows 10 x64 ZIP",
            "source_url":"https://www.intel.com/content/www/us/en/download/18799/intel-graphics-driver-for-windows-15-45.html",
            "archive_sha256":sha(args.zip),
            "comparison":"existing 21.20.16.4534 DriverStore vs official 21.20.16.5174 archive",
            "extracted":extracted,"modules":rows,
            "restrictions":"Static PE comparison only; does not execute driver, infer internal shader semantics, or show D3D9 game compatibility."}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print("REPORT",args.output)
    for item in rows:
        print(item["name"],"export delta",len(item.get("export_names_added",[])),
              len(item.get("export_names_removed",[])),"imports +/-",len(item.get("imports_added",[])),len(item.get("imports_removed",[])))

if __name__=="__main__":
    main()
