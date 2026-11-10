#!/usr/bin/env python3
"""Strict byte-counted HTTPS artifact downloader. Hard quota even without Content-Length.
No retry on unknown partial, no overwrite, and delete .part on failure.
"""
import argparse,hashlib,io,json,pathlib,urllib.request
from urllib.parse import urlparse
from datetime import datetime,timezone
def copy_bounded(src,dst,cap):
    if cap<1:raise ValueError("Maximum bytes must be positive")
    total=0
    while True:
        buf=src.read(min(65536,cap-total+1))
        if not buf:break
        if total+len(buf)>cap:
            raise ValueError("HARD_SIZE_LIMIT_EXCEEDED")
        dst.write(buf)
        total+=len(buf)
    return total
def self_test():
    for n,cap,ok in ((20,20,True),(21,20,False),(0,20,True)):
        src=io.BytesIO(b"x"*n)
        dest=io.BytesIO()
        try:
            transferred=copy_bounded(src,dest,cap)
            assert ok and transferred==n and len(dest.getvalue())<=cap
        except ValueError as e:
            assert not ok and str(e)=="HARD_SIZE_LIMIT_EXCEEDED" and len(dest.getvalue())<=cap
    print(json.dumps({"status":"STREAM_QUOTA_SELF_TEST_PASS","tests":3,
                      "hard_limit_enforced_without_content_length":True}))
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--self-test",action="store_true")
    ap.add_argument("--url")
    ap.add_argument("--out")
    ap.add_argument("--max-bytes",type=int)
    ap.add_argument("--expected-sha256")
    args=ap.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.url or not args.out or not args.max_bytes:
        ap.error("Provide --url --out and --max-bytes")
    parsed=urlparse(args.url)
    if parsed.scheme.lower()!="https" or not parsed.netloc:
        raise ValueError("HTTPS artifact source required")
    output=pathlib.Path(args.out).resolve()
    output.parent.mkdir(parents=True,exist_ok=True)
    partial=output.with_name(output.name+".part")
    receipt=output.with_name(output.name+".receipt.json")
    if output.exists() or partial.exists() or receipt.exists():
        raise FileExistsError("Destination, partial, or receipt exists; refuse overwrite")
    req=urllib.request.Request(args.url,headers={"User-Agent":"AIOS-Underdog-StreamCapped/1.0"})
    try:
        with urllib.request.urlopen(req,timeout=25) as response:
            redirected=urlparse(response.geturl())
            if redirected.scheme.lower()!="https":
                raise RuntimeError("Non-HTTPS redirect rejected")
            declared=response.headers.get("Content-Length")
            if declared is not None and int(declared)>args.max_bytes:
                raise ValueError("DECLARED_SIZE_EXCEEDS_LIMIT")
            with partial.open("xb") as dst:
                n=copy_bounded(response,dst,args.max_bytes)
        sha=hashlib.sha256(partial.read_bytes()).hexdigest()
        if args.expected_sha256 and sha.lower()!=args.expected_sha256.lower():
            raise RuntimeError("SHA256_MISMATCH")
        if output.exists():raise FileExistsError("Destination appeared during transfer")
        partial.rename(output)
        receipt.write_text(json.dumps({
            "schema":"aios.underdog.bounded_download.v1",
            "utc":datetime.now(timezone.utc).isoformat(),
            "source":args.url,"bytes":n,"max_bytes":args.max_bytes,
            "sha256":sha,"status":"COMPLETE_VERIFIED_QUOTA"
        },indent=2)+"\n",encoding="utf-8")
        print(json.dumps({"status":"DOWNLOADED_UNDER_HARD_CAP",
                          "bytes":n,"sha256":sha,"path":str(output)}))
    finally:
        if partial.exists():partial.unlink()
if __name__=="__main__":main()
