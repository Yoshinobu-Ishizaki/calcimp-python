#!/usr/bin/env python3
"""
Test XMENSUR validation - check for duplicate GROUPs and multiple MAINs
"""

import os
import sys
import subprocess

def test_duplicate_groups():
    """Test that duplicate GROUP names are detected"""
    test_content = """# Test duplicate groups
v1 = 1

MAIN
10,10,100
OPEN_END
END_MAIN

GROUP, TestGroup
12,12,50
OPEN_END
END_GROUP

GROUP, TestGroup
15,15,30
OPEN_END
END_GROUP
"""

    test_file = 'test/test_duplicate_groups.xmen'
    with open(test_file, 'w') as f:
        f.write(test_content)

    try:
        # Run python subprocess to capture stderr
        result = subprocess.run(
            [sys.executable, '-c', f'import calcimp; calcimp.print_men("{test_file}")'],
            capture_output=True,
            text=True
        )

        if result.returncode != 0 and "Duplicate GROUP" in result.stderr:
            print("✓ Duplicate GROUP correctly detected")
            return True
        else:
            print(f"✗ Duplicate GROUP test FAILED")
            print(f"  stderr: {result.stderr[:200]}")
            return False
    finally:
        if os.path.exists(test_file):
            os.remove(test_file)


def test_multiple_mains():
    """Test that multiple MAIN definitions are detected"""
    test_content = """# Test multiple MAINs
v1 = 1

MAIN
10,10,100
OPEN_END
END_MAIN

MAIN
12,12,50
OPEN_END
END_MAIN
"""

    test_file = 'test/test_multiple_mains.xmen'
    with open(test_file, 'w') as f:
        f.write(test_content)

    try:
        result = subprocess.run(
            [sys.executable, '-c', f'import calcimp; calcimp.print_men("{test_file}")'],
            capture_output=True,
            text=True
        )

        if result.returncode != 0 and "Multiple MAIN" in result.stderr:
            print("✓ Multiple MAIN definitions correctly detected")
            return True
        else:
            print(f"✗ Multiple MAIN test FAILED")
            print(f"  stderr: {result.stderr[:200]}")
            return False
    finally:
        if os.path.exists(test_file):
            os.remove(test_file)


def test_no_main():
    """Test that missing MAIN definition is detected"""
    test_content = """# Test no MAIN
v1 = 1

GROUP, TestGroup
12,12,50
OPEN_END
END_GROUP
"""

    test_file = 'test/test_no_main.xmen'
    with open(test_file, 'w') as f:
        f.write(test_content)

    try:
        result = subprocess.run(
            [sys.executable, '-c', f'import calcimp; calcimp.print_men("{test_file}")'],
            capture_output=True,
            text=True
        )

        if result.returncode != 0 and "No MAIN" in result.stderr:
            print("✓ Missing MAIN definition correctly detected")
            return True
        else:
            print(f"✗ No MAIN test FAILED")
            print(f"  stderr: {result.stderr[:200]}")
            return False
    finally:
        if os.path.exists(test_file):
            os.remove(test_file)


def test_valid_structure():
    """Test that valid structure passes validation"""
    test_content = """# Valid structure
v1 = 1

MAIN
10,10,100
OPEN_END
END_MAIN

GROUP, Group1
12,12,50
OPEN_END
END_GROUP

GROUP, Group2
15,15,30
OPEN_END
END_GROUP
"""

    test_file = 'test/test_valid_structure.xmen'
    with open(test_file, 'w') as f:
        f.write(test_content)

    try:
        import calcimp
        result = calcimp.print_men(test_file)
        print("✓ Valid structure correctly parsed")
        return True
    except Exception as e:
        print(f"✗ Valid structure test FAILED: {e}")
        return False
    finally:
        if os.path.exists(test_file):
            os.remove(test_file)


if __name__ == '__main__':
    print("Testing XMENSUR validation...")
    print("=" * 60)

    tests = [
        test_duplicate_groups,
        test_multiple_mains,
        test_no_main,
        test_valid_structure,
    ]

    results = []
    for test_func in tests:
        print(f"\nRunning {test_func.__name__}...")
        results.append(test_func())

    print()
    print("=" * 60)
    passed = sum(results)
    total = len(results)
    print(f"Results: {passed}/{total} tests passed")

    sys.exit(0 if all(results) else 1)
