#!/usr/bin/env python
"""
Test that both XMENSUR and ZMENSUR formats can be parsed successfully.

This test verifies that the parsers work correctly for both formats.
"""

import sys

try:
    import calcimp
except ImportError:
    print("Error: calcimp module not found. Please install it first.")
    sys.exit(1)


def test_format_parsing():
    """Test that both XMENSUR and ZMENSUR formats parse successfully."""

    print("=" * 70)
    print("Testing XMENSUR and ZMENSUR Format Parsing")
    print("=" * 70)

    # Test parameters
    max_freq = 500.0
    step_freq = 50.0

    print(f"\nTest parameters:")
    print(f"  Max frequency: {max_freq} Hz")
    print(f"  Frequency step: {step_freq} Hz")

    # Test 1: Simple ZMENSUR file
    print("\n1. Testing simple ZMENSUR format...")
    print("   File: ../sample/test.men")
    try:
        freq, real, imag, mag = calcimp.calcimp(
            '../sample/test.men',
            max_freq=max_freq,
            step_freq=step_freq
        )
        print(f"   ✓ Success: {len(freq)} points calculated")
        print(f"   Sample magnitudes: {mag[:3]}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
        return False

    # Test 2: Simple XMENSUR file
    print("\n2. Testing simple XMENSUR format...")
    print("   File: ../sample/test.xmen")
    try:
        freq, real, imag, mag = calcimp.calcimp(
            '../sample/test.xmen',
            max_freq=max_freq,
            step_freq=step_freq
        )
        print(f"   ✓ Success: {len(freq)} points calculated")
        print(f"   Sample magnitudes: {mag[:3]}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
        return False

    # Test 3: Complex XMENSUR file with variables and groups
    print("\n3. Testing complex XMENSUR format (with variables & groups)...")
    print("   File: sample_xmensur.xmen")
    try:
        freq, real, imag, mag = calcimp.calcimp(
            'sample_xmensur.xmen',
            max_freq=max_freq,
            step_freq=step_freq
        )
        print(f"   ✓ Success: {len(freq)} points calculated")
        print(f"   Sample magnitudes: {mag[:3]}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
        return False

    # Test 4: Valve example
    print("\n4. Testing trumpet with valve (XMENSUR)...")
    print("   File: ../sample/trumpet_valve.xmen")
    try:
        freq, real, imag, mag = calcimp.calcimp(
            '../sample/trumpet_valve.xmen',
            max_freq=max_freq,
            step_freq=step_freq
        )
        print(f"   ✓ Success: {len(freq)} points calculated")
        print(f"   Sample magnitudes: {mag[:3]}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
        return False

    print("\n" + "=" * 70)
    print("✓ ALL TESTS PASSED: Both formats parse successfully")
    print("=" * 70)

    return True


if __name__ == '__main__':
    success = test_format_parsing()
    sys.exit(0 if success else 1)
