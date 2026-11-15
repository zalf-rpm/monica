#!/usr/bin/env python3
"""Compare a C++ and an Odin daily section when the C++ prints Mois with 3
digits and the Odin with 6 (diff_vs_cpp.sh's note): the Odin Mois_1..3 values
rounded to 3 digits must equal the C++'s - or differ by at most half a unit in
the last digit, where rounding the 6-digit value and rounding the full double
can disagree. Every other column must be identical.

    pixi run python tests/compare_mois_digits.py <cpp daily.csv> <odin daily.csv>
"""
import sys
from decimal import Decimal, ROUND_HALF_UP

cpp = open(sys.argv[1]).read().splitlines()
odin = open(sys.argv[2]).read().splitlines()
if len(cpp) != len(odin):
    sys.exit(f"different number of lines: {len(cpp)} vs {len(odin)}")
hdr = cpp[0].split(",")
mois = [i for i, h in enumerate(hdr) if h in ("Mois_1", "Mois_2", "Mois_3")]
bad = near = 0
for n, (a, b) in enumerate(zip(cpp, odin)):
    if a == b:
        continue
    ca, ob = a.split(","), b.split(",")
    for i in range(len(ca)):
        if ca[i] == ob[i]:
            continue
        if i in mois:
            r = str(Decimal(ob[i]).quantize(Decimal("0.001"), ROUND_HALF_UP).normalize())
            if r == ca[i] or float(r) == float(ca[i]):
                continue
            if abs(float(ob[i]) - float(ca[i])) <= 0.0005 + 1e-9:
                near += 1
                continue
        bad += 1
        if bad <= 10:
            print(f"line {n + 1} col {hdr[i]}: cpp={ca[i]} odin={ob[i]}")
print(f"lines {len(cpp)}, real differences {bad}, Mois rounding ties {near}")
sys.exit(1 if bad else 0)
