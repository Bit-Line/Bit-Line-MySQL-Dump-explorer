#!/usr/bin/env python3
"""Statically validate the Windows PE build and its dependency/resource metadata.
This does not launch the executable or validate runtime availability of individual APIs.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
ALLOWED={'kernel32.dll','user32.dll','gdi32.dll','comctl32.dll','comdlg32.dll','shell32.dll','uxtheme.dll','msvcrt.dll'}

def inspect(path: Path) -> dict:
    b=path.read_bytes()
    def u16(p: int) -> int:return struct.unpack_from('<H',b,p)[0]
    def u32(p: int) -> int:return struct.unpack_from('<I',b,p)[0]
    def u64(p: int) -> int:return struct.unpack_from('<Q',b,p)[0]
    def check(ok: bool,message: str) -> None:
        if not ok:raise ValueError(message)
    check(b[:2]==b'MZ','Missing DOS signature');pe=u32(0x3c)
    check(b[pe:pe+4]==b'PE\0\0','Missing PE signature');check(u16(pe+4)==0x8664,'Not x64')
    section_count=u16(pe+6);opt=pe+24;check(u16(opt)==0x20b,'Not PE32+')
    check(u16(opt+68)==2,'Not Windows GUI');flags=u16(opt+70)
    check(flags&0x20 and flags&0x40 and flags&0x100,'Missing High-Entropy ASLR / ASLR / DEP')
    directory=opt+112
    os_version=(u16(opt+40),u16(opt+42))
    subsystem_version=(u16(opt+48),u16(opt+50))
    load_config_rva=u32(directory+10*8);load_config_size=u32(directory+10*8+4)
    check(not(subsystem_version>=(6,3) and (not load_config_rva or not load_config_size)),
          'Invalid loader contract: subsystem >= 6.3 without load configuration / GS cookie. '
          'This can cause Windows startup error 0xC000007B. Rebuild with tools/build.py.')
    check(subsystem_version==(6,0) and os_version==(6,0),
          'Unexpected loader version: this CRT-minimal project requires /subsystem:windows,6.00 /osversion:6.00')
    section_table=opt+u16(pe+20);sections=[]
    for i in range(section_count):
        p=section_table+40*i
        name=b[p:p+8].split(b'\0',1)[0].decode('ascii')
        sections.append((name,u32(p+12),max(u32(p+8),u32(p+16)),u32(p+20),u32(p+16)))
    def rva(value: int) -> int:
        for name,va,size,raw,raw_size in sections:
            if va<=value<va+size:
                off=raw+value-va;check(off<len(b),'RVA exceeds file');return off
        raise ValueError(f'RVA not mapped: {value:x}')
    def cstr(offset: int) -> str:return b[offset:b.index(b'\0',offset)].decode('ascii')
    check(any(va<=u32(opt+16)<va+size and name=='.text' for name,va,size,raw,raw_size in sections),'Invalid entry point')
    imports=[];p=rva(u32(directory+8))
    while u32(p) or u32(p+12) or u32(p+16):
        dll=cstr(rva(u32(p+12)));check(dll.lower() in ALLOWED,f'Unexpected runtime dependency: {dll}')
        thunk=rva(u32(p) or u32(p+16));functions=[]
        while u64(thunk):
            v=u64(thunk);check(not (v>>63),'Unexpected ordinal import');functions.append(cstr(rva(v)+2));thunk+=8
        imports.append({'dll':dll,'functions':functions});p+=20
    check(set(i['dll'].lower() for i in imports)==ALLOWED,'Dependency list differs from expected OS-only set')
    resources=[];resource_base=rva(u32(directory+16))
    def walk(offset: int,parents: list[int]) -> None:
        p=resource_base+offset;count=u16(p+12)+u16(p+14)
        for i in range(count):
            entry=p+16+i*8;name=u32(entry);dest=u32(entry+4)
            check(not(name&0x80000000),'Unexpected named resource')
            if dest&0x80000000:walk(dest&0x7fffffff,parents+[name])
            else:
                d=resource_base+dest;body=rva(u32(d));size=u32(d+4)
                check(body+size<=len(b),'Resource exceeds file')
                resources.append({'path':parents+[name],'bytes':size})
                if parents and parents[0]==24:
                    xml=b[body:body+size].decode('utf-8')
                    root=ET.fromstring(xml)
                    ns={'a':'urn:schemas-microsoft-com:asm.v1'}
                    identity=root.find('a:assemblyIdentity',ns)
                    check(identity is not None and identity.get('processorArchitecture')=='amd64','Application manifest must target amd64')
                    for dep in root.findall('a:dependency/a:dependentAssembly/a:assemblyIdentity',ns):
                        check(dep.get('processorArchitecture') in ('amd64','*'),'Manifest dependency architecture mismatch')
                    check('asInvoker' in xml and 'PerMonitorV2' in xml,'Manifest mismatch')
    walk(0,[]);types={r['path'][0] for r in resources}
    check({2,3,14,16,24}.issubset(types),'Bitmap / icon / version / manifest resources missing')
    signed=bool(u32(directory+4*8))
    return {'file':path.name,'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest(),'format':'PE32+ x64 Windows GUI',
            'minimum_subsystem':list(subsystem_version),'minimum_os_header':list(os_version),
            'load_config_directory':{'rva':load_config_rva,'bytes':load_config_size},
            'loader_policy':'CRT-minimal x64 desktop; subsystem and OS header 6.0',
            'runtime_os_requirement':'Windows 10 1703+ x64 / Windows 11 x64 (unchanged)','high_entropy_aslr':True,'aslr':True,'dep':True,'signed':signed,
            'sections':section_count,'imports':imports,'resources':resources,'executed_on_windows':False}

def main() -> None:
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('exe',nargs='?',type=Path,default=ROOT/'build/windows/BitLineDumpBrowser.exe');args=ap.parse_args()
    print(json.dumps(inspect(args.exe),indent=2))

if __name__=='__main__':main()
