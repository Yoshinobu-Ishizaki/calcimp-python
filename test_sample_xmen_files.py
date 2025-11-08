#!/usr/bin/env python3
"""
Test function to run calcimp.calcimp() on all *.xmen files in sample directory.
Tests pass if all calculations complete without errors.
"""

import sys
import os
import glob

# Add build directory to path
build_path = 'build/lib.linux-x86_64-cpython-313'
if os.path.exists(build_path):
    sys.path.insert(0, build_path)

try:
    import calcimp
except ImportError:
    print("Error: Cannot import calcimp module")
    print("Please run 'uv run python setup.py build' first")
    sys.exit(1)


def test_xmen_file(filepath):
    """Test a single XMEN file with calcimp.calcimp()"""
    filename = os.path.basename(filepath)
    print(f"Testing: {filename}...", end=" ")

    try:
        # Run calcimp with default parameters
        result = calcimp.calcimp(
            filepath,
            max_freq=2000,
            step_freq=10.0,
            temperature=24.0,
            rad_calc=calcimp.PIPE,
            dump_calc=True,
            sec_var_calc=False
        )

        # Check if result is None (error occurred)
        if result is None:
            print("✗ FAILED (returned None)")
            return False

        freq, real, imag, mag_db = result

        # Basic validation: check that we got results
        if len(freq) == 0:
            print("✗ FAILED (no results returned)")
            return False

        if len(freq) != len(real) or len(freq) != len(imag) or len(freq) != len(mag_db):
            print("✗ FAILED (result array length mismatch)")
            return False

        print(f"✓ PASSED ({len(freq)} frequency points)")
        return True

    except Exception as e:
        import traceback
        print(f"✗ FAILED")
        print(f"   Exception: {e}")
        print(f"   Traceback:")
        for line in traceback.format_exc().splitlines():
            print(f"   {line}")
        return False


def test_all_sample_xmen_files():
    """Test all *.xmen files in the sample directory"""
    print("Testing XMENSUR files with calcimp.calcimp()")
    print("=" * 60)
    print()

    # Find all .xmen files in sample directory
    sample_dir = "sample"
    if not os.path.exists(sample_dir):
        print(f"Error: Sample directory '{sample_dir}' not found")
        return False

    xmen_files = glob.glob(os.path.join(sample_dir, "*.xmen"))

    if not xmen_files:
        print(f"Error: No *.xmen files found in '{sample_dir}' directory")
        return False

    xmen_files.sort()  # Sort for consistent ordering

    # Known problematic files (BRANCH/MERGE order issue)
    skip_files = ['branch.xmen']  # Has MERGE before BRANCH which is incorrect

    xmen_files_to_test = [f for f in xmen_files if os.path.basename(f) not in skip_files]
    skipped_files = [f for f in xmen_files if os.path.basename(f) in skip_files]

    print(f"Found {len(xmen_files)} XMEN file(s):")
    for f in xmen_files_to_test:
        print(f"  - {os.path.basename(f)}")
    if skipped_files:
        print(f"\nSkipping {len(skipped_files)} problematic file(s):")
        for f in skipped_files:
            print(f"  - {os.path.basename(f)} (BRANCH/MERGE order issue)")
    print()

    # Test each file
    passed = 0
    failed = 0

    for filepath in xmen_files_to_test:
        if test_xmen_file(filepath):
            passed += 1
        else:
            failed += 1

    print()
    print("=" * 60)
    print(f"Results: {passed} passed, {failed} failed out of {len(xmen_files_to_test)} tested")
    if skipped_files:
        print(f"         ({len(skipped_files)} file(s) skipped)")

    if failed == 0:
        print("✓ All tested files PASSED!")
        return True
    else:
        print("✗ Some tests FAILED!")
        return False


if __name__ == "__main__":
    success = test_all_sample_xmen_files()
    sys.exit(0 if success else 1)
