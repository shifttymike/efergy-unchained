#!/usr/bin/env python3
"""Read diagnostic firmware status over HTTP; does not change device settings."""
import argparse
import json
import time
import urllib.request
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument("--host",default="192.168.23.13")
parser.add_argument("--samples",type=int,default=8)
parser.add_argument("--period",type=float,default=3)
parser.add_argument("--output",type=Path,default=Path("build/status.jsonl"))
args=parser.parse_args()
if not 1<=args.samples<=10000 or args.period<0.1:parser.error("invalid sample count or period")
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
args.output.parent.mkdir(parents=True,exist_ok=True)
failures=0
with args.output.open("a") as output:
    for i in range(args.samples):
        try:
            with opener.open("http://"+args.host+"/status",timeout=6) as response:status=json.load(response)
            row={"time":time.time(),"status":status}
            output.write(json.dumps(row)+"\n");output.flush()
            print(json.dumps(status),flush=True)
        except Exception as exc:
            failures+=1
            print(type(exc).__name__+": "+str(exc),flush=True)
        if i+1<args.samples:time.sleep(args.period)
raise SystemExit(1 if failures else 0)
