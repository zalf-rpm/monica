#!/usr/bin/env python3
"""Byte-compare every CSV under two directories, e.g. the work directories of
tests/check_drivers.py run with a reference and a new binary.

    pixi run python tests/compare_csv_dirs.py <reference dir> <new dir>
"""
import sys
from pathlib import Path

a, b = Path(sys.argv[1]), Path(sys.argv[2])
fa = {p.relative_to(a) for p in a.rglob("*.csv")}
fb = {p.relative_to(b) for p in b.rglob("*.csv")}
only = sorted(fa ^ fb)
diff = sorted(r for r in fa & fb if (a / r).read_bytes() != (b / r).read_bytes())
print(f"{len(fa & fb)} CSVs in both, {len(diff)} differ, {len(only)} only in one")
for r in diff[:20]:
    print("  DIFFERS", r)
for r in only[:20]:
    print("  ONLY   ", r, "(reference)" if r in fa else "(new)")
sys.exit(1 if diff or only else 0)
