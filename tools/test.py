#!/usr/bin/env python3
"""Run the portable parser tests on Linux with GCC, ASan and UBSan (stdlib only)."""
from __future__ import annotations
import os
from pathlib import Path
import shutil
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]

def main() -> None:
    if sys.platform!='linux':raise SystemExit('This sanitizer runner targets Linux. Use docs/WINDOWS-ABNAHME.md for the GUI checks.')
    cc=shutil.which('gcc')
    if not cc:raise SystemExit('GCC is required for this developer test command.')
    build=ROOT/'build';build.mkdir(exist_ok=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1')
    for name,args in [('test_core',[]),('test_large_offsets',[]),('inspect',['samples/bitline-demo.sql','build/test-data/cache'])]:
        exe=build/name
        command=[cc,'-std=c17','-D_POSIX_C_SOURCE=200809L','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','src/core.c','src/platform.c',f'tests/{name}.c','-o',str(exe)]
        subprocess.run(command,cwd=ROOT,check=True)
        subprocess.run([str(exe),*args],cwd=ROOT,env=env,check=True)
    print('All headless sanitizer checks passed. This is NOT a Windows GUI test.')

if __name__=='__main__':main()
