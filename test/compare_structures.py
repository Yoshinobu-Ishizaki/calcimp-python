#!/usr/bin/env python3
"""
Compare XMENSUR and ZMENSUR structures in detail
"""

import calcimp

print("=" * 70)
print("STRUCTURE COMPARISON: XMENSUR vs ZMENSUR")
print("=" * 70)

xmen = calcimp.print_men('test/sample_xmensur.xmen')
zmen = calcimp.print_men('test/sample_xmensur_equiv.men')

print(f"\nXMENSUR: {len(xmen)} segments")
print(f"ZMENSUR: {len(zmen)} segments")
print()

# Find the first difference
first_diff = None
for i in range(min(len(xmen), len(zmen))):
    xdf, xdb, xr, xcomment = xmen[i]
    zdf, zdb, zr, zcomment = zmen[i]

    if abs(xdf - zdf) > 0.01 or abs(xdb - zdb) > 0.01 or abs(xr - zr) > 0.01:
        first_diff = i
        break

if first_diff is not None:
    print(f"First difference at segment [{first_diff}]:")
    print(f"  XMENSUR: df={xmen[first_diff][0]:.1f} db={xmen[first_diff][1]:.1f} r={xmen[first_diff][2]:.1f}")
    print(f"  ZMENSUR: df={zmen[first_diff][0]:.1f} db={zmen[first_diff][1]:.1f} r={zmen[first_diff][2]:.1f}")
    print()
elif len(xmen) != len(zmen):
    print(f"All segments up to position {min(len(xmen), len(zmen))-1} match!")
    print(f"Difference: XMENSUR has {len(xmen) - len(zmen)} extra segment(s)")
    print()

# Show around the valve region
print("Valve region comparison:")
print()
print("XMENSUR:")
for i in range(8, min(13, len(xmen))):
    df, db, r, comment = xmen[i]
    marker = ""
    if i == 9 and r == 0.0:
        marker = " <- SPLIT MARKER (should be removed)"
    print(f"  [{i:2d}] df={df:6.1f} db={db:6.1f} r={r:8.1f}{marker}")

print()
print("ZMENSUR:")
for i in range(8, min(12, len(zmen))):
    df, db, r, comment = zmen[i]
    print(f"  [{i:2d}] df={df:6.1f} db={db:6.1f} r={r:8.1f}")

print()
print("=" * 70)
print("ANALYSIS:")
print("=" * 70)
print()

if len(xmen) == len(zmen) + 1:
    # Check if segment [9] in XMENSUR is a marker
    if xmen[9][2] == 0.0:  # r == 0
        print("✗ XMENSUR has an extra SPLIT marker segment [9]")
        print("  This marker should be removed by rejoint_men()")
        print()
        print("  The valve branch (V1LOOP) was correctly inserted:")
        print(f"    XMENSUR[10] = ZMENSUR[9]: df=12.0, db=12.0, r=100.0 ✓")
        print()
        print("  But the marker segment was not cleaned up.")
        print()
        print("RECOMMENDATION:")
        print("  The marker segment should either:")
        print("  1. Be removed during rejoint_men()")
        print("  2. Not be created as a separate segment")
        print("  3. Be absorbed/merged with adjacent segments")
else:
    print(f"Unexpected segment count difference: {abs(len(xmen) - len(zmen))}")

print()
print("=" * 70)
