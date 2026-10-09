#!/usr/bin/env python3
"""Build the Windows x64 executable with Python's standard library + LLVM.
No Windows SDK, NuGet, npm, network access, or third-party Python packages needed.
Usage: python tools/build.py [--clang PATH] [--link PATH]
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess

from verify_pe import inspect as inspect_pe

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build' / 'windows'

def pad4(b: bytes) -> bytes:
    return b + b'\0' * ((-len(b)) % 4)

def resource(type_id: int, name_id: int, data: bytes) -> bytes:
    names = struct.pack('<HHHH', 0xffff, type_id, 0xffff, name_id)
    tail = struct.pack('<IHHII', 0, 0x1030, 0x409, 0, 0)
    header = struct.pack('<II', len(data), 8 + len(names) + len(tail)) + names + tail
    return header + pad4(data)

def version_block(key: str, value: bytes = b'', children: bytes = b'', text: bool = False) -> bytes:
    keybytes = (key + '\0').encode('utf-16le')
    start = pad4(struct.pack('<HHH', 0, len(value)//2 if text else len(value), int(text)) + keybytes)
    block = start + value
    if children:
        block = pad4(block) + children
    return struct.pack('<H', len(block)) + block[2:]

def make_resources() -> Path:
    result = struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0, 0xffff, 0, 0, 0, 0, 0, 0)
    result += resource(24, 1, (ROOT/'assets/app.manifest').read_bytes())
    result += resource(2, 102, (ROOT/'assets/wordmark.bmp').read_bytes()[14:])
    next_image = 1
    for filename, group in [('app.ico',101),('database.ico',103),('table.ico',104),('code.ico',105)]:
        icon = (ROOT/'assets'/filename).read_bytes()
        reserved, kind, count = struct.unpack_from('<HHH', icon)
        if reserved or kind != 1:
            raise ValueError(f'Invalid icon: {filename}')
        entries = struct.pack('<HHH', 0, 1, count)
        for i in range(count):
            w,h,colors,reserved,planes,bits,size,offset = struct.unpack_from('<BBBBHHII', icon, 6+16*i)
            entries += struct.pack('<BBBBHHIH',w,h,colors,reserved,planes,bits,size,next_image)
            result += resource(3, next_image, icon[offset:offset+size])
            next_image += 1
        result += resource(14, group, entries)
    fixed = struct.pack('<13I',0xfeef04bd,0x10000,0x10000,0x10000,0x10000,0x10000,0x3f,0,0x40004,1,0,0,0)
    values = {
        'CompanyName':'Bit-Line', 'FileDescription':'Bit-Line Dump Browser – lokaler MySQL-Dumpbrowser',
        'FileVersion':'1.0.1.0', 'InternalName':'BitLineDumpBrowser',
        'OriginalFilename':'BitLineDumpBrowser.exe','ProductName':'Bit-Line Dump Browser',
        'ProductVersion':'1.0.1','LegalCopyright':'Bit-Line Dump Browser contributors, 2026'
    }
    strings = b''.join(pad4(version_block(k,(v+'\0').encode('utf-16le'),text=True)) for k,v in values.items())
    string_info = version_block('StringFileInfo',children=version_block('040904B0',children=strings))
    var_info = version_block('VarFileInfo',children=version_block('Translation',struct.pack('<HH',0x409,1200)))
    result += resource(16,1,version_block('VS_VERSION_INFO',fixed,pad4(string_info)+var_info))
    path=BUILD/'app.res';path.write_bytes(result);return path

def run(args: list[str]) -> None:
    print(' '.join(map(str,args)), flush=True)
    subprocess.run(args, cwd=ROOT, check=True)

def main() -> None:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--clang',default=shutil.which('clang'))
    ap.add_argument('--link',default=shutil.which('lld-link'))
    args=ap.parse_args()
    if not args.clang or not args.link:
        ap.error('LLVM clang and lld-link must be installed and on PATH.')
    BUILD.mkdir(parents=True,exist_ok=True)
    names = re.findall(r'^API\s+.*?\b(?:WINAPI)\s+(\w+)\(', (ROOT/'src/winmini.h').read_text(), re.M)
    # Pointer-return declarations may have an asterisk immediately before WINAPI; the same regex covers them.
    kernel=set('ExitProcess GetModuleHandleW GetCommandLineW GetLastError CreateFileW CloseHandle ReadFile WriteFile SetFilePointerEx GetFileSizeEx GetFileTime CreateDirectoryW DeleteFileW MoveFileExW GetTickCount64 FlushFileBuffers MultiByteToWideChar WideCharToMultiByte GetFullPathNameW GetEnvironmentVariableW GetModuleFileNameW GetTempPathW GetCurrentProcessId CreateThread Sleep GlobalAlloc GlobalLock GlobalUnlock GlobalFree LocalFree'.split())
    gdi=set('CreateFontW CreateSolidBrush DeleteObject SelectObject SetBkMode SetTextColor GetStockObject CreateCompatibleDC DeleteDC GetObjectW StretchBlt SetStretchBltMode CreatePen MoveToEx LineTo RoundRect'.split())
    groups={
        'kernel32': sorted(kernel), 'gdi32':sorted(gdi),
        'comctl32':['InitCommonControlsEx','ImageList_Create','ImageList_ReplaceIcon','ImageList_Destroy'],
        'comdlg32':['GetOpenFileNameW','GetSaveFileNameW'],
        'shell32':['DragAcceptFiles','DragQueryFileW','DragFinish','ShellExecuteW','CommandLineToArgvW'],
        'uxtheme':['SetWindowTheme'],
        'msvcrt':['malloc','calloc','realloc','free','memcpy','memmove','memset','memcmp','memchr','strlen','strcmp','strncmp','strstr','strchr','_vsnprintf'],
    }
    covered=set().union(*map(set,groups.values()))
    groups['user32']=sorted(set(names)-covered)
    libs=[]
    for dll,exports in groups.items():
        definition=BUILD/f'{dll}.def';definition.write_text(f'LIBRARY {dll}.dll\nEXPORTS\n'+'\n'.join(exports)+'\n')
        lib=BUILD/f'{dll}.lib';run([args.link,'/lib',f'/def:{definition}','/machine:x64',f'/out:{lib}']);libs.append(str(lib))
    objects=[]
    for source in ['platform.c','core.c','app.c','chkstk.S']:
        obj=BUILD/(Path(source).stem+'.obj')
        command=[args.clang,'--target=x86_64-pc-windows-msvc','-O2','-fno-stack-protector','-c',str(ROOT/'src'/source),'-o',str(obj)]
        if source.endswith('.c'):
            command[2:2]=['-std=c17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-pointer-sign']
        run(command);objects.append(str(obj))
    res=make_resources()
    exe=BUILD/'BitLineDumpBrowser.exe'
    # Use the standard x64 desktop loader contract. This CRT-minimal build has
    # no IMAGE_LOAD_CONFIG_DIRECTORY / GS cookie. Subsystem >= 6.3 can reject
    # that combination with STATUS_INVALID_IMAGE_FORMAT (0xC000007B).
    # This header is NOT the API/OS support promise: runtime still needs Win10 1703+.
    run([args.link,'/machine:x64','/subsystem:windows,6.00','/osversion:6.00','/entry:WinMainCRTStartup','/nodefaultlib',
        '/dynamicbase','/highentropyva','/nxcompat','/opt:ref','/opt:icf','/timestamp:0','/stack:8388608',
        f'/out:{exe}',*objects,str(res),*libs])
    report = inspect_pe(exe)
    print(f'Built and statically checked: {exe} ({exe.stat().st_size:,} bytes)')
    print(f"SHA-256: {report['sha256']}")
    print('Native Windows startup has NOT been tested by this build command.')

if __name__ == '__main__':
    main()
