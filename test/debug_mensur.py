#!/usr/bin/env python
"""
Debug script to print mensur segment details for comparison.
"""
import sys
sys.path.insert(0, '..')

# We need to add C-level debug printing to see the mensur structures
# For now, let's just verify the Python side is working

import calcimp

print("=" * 70)
print("Testing XMENSUR parsing")
print("=" * 70)

try:
    freq1, real1, imag1, mag1 = calcimp.calcimp(
        'sample_xmensur.xmen',
        max_freq=50,
        step_freq=50
    )
    print(f"✓ XMENSUR parsed successfully")
    print(f"  Impedance at 50Hz: real={real1[1]:.6f}, imag={imag1[1]:.6f}, mag={mag1[1]:.6f}")
except Exception as e:
    print(f"✗ Error: {e}")
    sys.exit(1)

print("\n" + "=" * 70)
print("Testing ZMENSUR parsing")
print("=" * 70)

try:
    freq2, real2, imag2, mag2 = calcimp.calcimp(
        'sample_xmensur_equiv.men',
        max_freq=50,
        step_freq=50
    )
    print(f"✓ ZMENSUR parsed successfully")
    print(f"  Impedance at 50Hz: real={real2[1]:.6f}, imag={imag2[1]:.6f}, mag={mag2[1]:.6f}")
except Exception as e:
    print(f"✗ Error: {e}")
    sys.exit(1)

print("\n" + "=" * 70)
print("Comparison")
print("=" * 70)
print(f"Real difference: {abs(real1[1] - real2[1]):.6f}")
print(f"Imag difference: {abs(imag1[1] - imag2[1]):.6f}")
print(f"Mag difference: {abs(mag1[1] - mag2[1]):.6f}")

if abs(real1[1] - real2[1]) < 1e-5 and abs(imag1[1] - imag2[1]) < 1e-5:
    print("\n✓ Results match!")
else:
    print("\n✗ Results differ significantly")
    print("\nThis indicates a bug in the xmensur.c parser.")
    print("The parser is not creating the same mensur structure as zmensur.c")
