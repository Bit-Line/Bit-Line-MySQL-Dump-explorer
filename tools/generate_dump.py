#!/usr/bin/env python3
"""Generate deterministic synthetic MySQL fixtures; streaming, standard library only."""
from __future__ import annotations
import argparse
from pathlib import Path

def main() -> None:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('output',type=Path)
    ap.add_argument('--rows',type=int,default=1_000_000)
    args=ap.parse_args()
    if args.rows<1: ap.error('--rows must be positive')
    with args.output.open('w',encoding='utf-8',newline='\n',buffering=1024*1024) as f:
        f.write("CREATE DATABASE `benchmark`; USE `benchmark`;\n")
        f.write("CREATE TABLE `events` (`id` BIGINT UNSIGNED, `category` VARCHAR(30), `amount` DECIMAL(20,6), `payload` TEXT, `extra` VARBINARY(64), `created_at` DATETIME);\n")
        f.write("INSERT INTO `events` VALUES\n")
        for i in range(args.rows):
            f.write(f"({i},'category_{i%100:02d}',12345678901234.123456,'Grüße aus Bit-Line; commas, brackets (x), quote \\' and line\\nnext',0x00FFABCD,'2026-01-01 12:00:00')")
            f.write(';\n' if i+1==args.rows else ',\n')
    print(f'{args.output}: {args.rows:,} rows, {args.output.stat().st_size:,} bytes; one extended INSERT')

if __name__=='__main__': main()
