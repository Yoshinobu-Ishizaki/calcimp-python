#!/usr/bin/env python
"""
Test that XMENSUR and ZMENSUR equivalent files produce identical results.

This test verifies that sample_xmensur.xmen and sample_xmensur_equiv.men
generate the same acoustic impedance calculations.
"""

import sys
import numpy as np

try:
    import calcimp
except ImportError:
    print("Error: calcimp module not found. Please install it first.")
    sys.exit(1)


def test_xmensur_zmensur_equivalence():
    """Test that XMENSUR and equivalent ZMENSUR produce identical results."""

    print("=" * 70)
    print("Testing XMENSUR vs ZMENSUR Equivalence")
    print("=" * 70)

    # Test parameters
    max_freq = 1000.0
    step_freq = 10.0
    temperature = 24.0

    print(f"\nTest parameters:")
    print(f"  Max frequency: {max_freq} Hz")
    print(f"  Frequency step: {step_freq} Hz")
    print(f"  Temperature: {temperature}°C")

    # Calculate impedance for XMENSUR file
    print("\n1. Calculating impedance for XMENSUR format...")
    print("   File: sample_xmensur.xmen")
    try:
        freq_xmen, real_xmen, imag_xmen, mag_xmen = calcimp.calcimp(
            'sample_xmensur.xmen',
            max_freq=max_freq,
            step_freq=step_freq,
            temperature=temperature
        )
        print(f"   ✓ Success: {len(freq_xmen)} frequency points calculated")
    except Exception as e:
        print(f"   ✗ Error: {e}")
        return False

    # Calculate impedance for ZMENSUR equivalent file
    print("\n2. Calculating impedance for ZMENSUR format...")
    print("   File: sample_xmensur_equiv.men")
    try:
        freq_men, real_men, imag_men, mag_men = calcimp.calcimp(
            'sample_xmensur_equiv.men',
            max_freq=max_freq,
            step_freq=step_freq,
            temperature=temperature
        )
        print(f"   ✓ Success: {len(freq_men)} frequency points calculated")
    except Exception as e:
        print(f"   ✗ Error: {e}")
        return False

    # Compare results
    print("\n3. Comparing results...")

    # Check array lengths
    if len(freq_xmen) != len(freq_men):
        print(f"   ✗ Array length mismatch: {len(freq_xmen)} vs {len(freq_men)}")
        return False
    print(f"   ✓ Array lengths match: {len(freq_xmen)} points")

    # Check frequencies match
    if not np.allclose(freq_xmen, freq_men, rtol=1e-10):
        print("   ✗ Frequencies don't match")
        max_diff = np.max(np.abs(freq_xmen - freq_men))
        print(f"     Max difference: {max_diff}")
        return False
    print("   ✓ Frequencies match exactly")

    # Check real parts
    rtol = 1e-5  # Relative tolerance
    atol = 1e-8  # Absolute tolerance

    if not np.allclose(real_xmen, real_men, rtol=rtol, atol=atol):
        print(f"   ✗ Real parts don't match (rtol={rtol}, atol={atol})")
        max_diff = np.max(np.abs(real_xmen - real_men))
        max_rel_diff = np.max(np.abs((real_xmen - real_men) / (np.abs(real_men) + 1e-10)))
        print(f"     Max absolute difference: {max_diff}")
        print(f"     Max relative difference: {max_rel_diff}")
        return False
    print(f"   ✓ Real parts match (rtol={rtol}, atol={atol})")

    # Check imaginary parts
    if not np.allclose(imag_xmen, imag_men, rtol=rtol, atol=atol):
        print(f"   ✗ Imaginary parts don't match (rtol={rtol}, atol={atol})")
        max_diff = np.max(np.abs(imag_xmen - imag_men))
        max_rel_diff = np.max(np.abs((imag_xmen - imag_men) / (np.abs(imag_men) + 1e-10)))
        print(f"     Max absolute difference: {max_diff}")
        print(f"     Max relative difference: {max_rel_diff}")
        return False
    print(f"   ✓ Imaginary parts match (rtol={rtol}, atol={atol})")

    # Check magnitudes
    if not np.allclose(mag_xmen, mag_men, rtol=rtol, atol=atol):
        print(f"   ✗ Magnitudes don't match (rtol={rtol}, atol={atol})")
        max_diff = np.max(np.abs(mag_xmen - mag_men))
        print(f"     Max absolute difference: {max_diff}")
        return False
    print(f"   ✓ Magnitudes match (rtol={rtol}, atol={atol})")

    # Print some statistics
    print("\n4. Statistics:")
    print(f"   Frequency range: {freq_xmen[0]:.1f} - {freq_xmen[-1]:.1f} Hz")
    print(f"   Magnitude range: {np.min(mag_xmen):.2f} - {np.max(mag_xmen):.2f} dB")

    # Find resonance peaks (local maxima above threshold)
    threshold = 60.0  # dB
    peaks = []
    for i in range(1, len(mag_xmen) - 1):
        if (mag_xmen[i] > mag_xmen[i-1] and
            mag_xmen[i] > mag_xmen[i+1] and
            mag_xmen[i] > threshold):
            peaks.append((freq_xmen[i], mag_xmen[i]))

    if peaks:
        print(f"\n   Found {len(peaks)} resonance peaks above {threshold} dB:")
        for i, (f, m) in enumerate(peaks[:5], 1):
            print(f"     Peak {i}: {f:.1f} Hz ({m:.2f} dB)")
        if len(peaks) > 5:
            print(f"     ... and {len(peaks) - 5} more peaks")

    print("\n" + "=" * 70)
    print("✓ TEST PASSED: XMENSUR and ZMENSUR produce identical results")
    print("=" * 70)

    return True


if __name__ == '__main__':
    print("\nNOTE: Full equivalence testing requires manual verification")
    print("that the ZMENSUR file correctly represents the XMENSUR structure.")
    print("Current test verifies both formats can be parsed successfully.\n")

    success = test_xmensur_zmensur_equivalence()
    sys.exit(0 if success else 1)
