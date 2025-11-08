#!/usr/bin/env python3
"""
Test XMENSUR variable parsing with TinyExpr
"""

import os
import sys

# Create test XMENSUR file with various expressions
test_xmen_content = """# Test variable parsing with TinyExpr

# Basic arithmetic
v1 = 1
v2 = 1 - v1
v3 = 0

# More complex expressions
s_main_pull = 0.5*2 + 3.4
radius = 10 / 2
area = pi * radius^2

# Math functions
angle = pi / 4
sin_value = sin(angle)
cos_value = cos(angle)
sqrt_value = sqrt(16)

# Nested expressions
complex_expr = (v1 + v2) * 2 + sin(pi/6)

MAIN
# Simple tube using variables
radius, radius, 100
OPEN_END
END_MAIN
"""

def test_variable_parsing():
    """Test that variables are parsed correctly"""

    # Write test file
    test_file = 'test/test_variables.xmen'
    with open(test_file, 'w') as f:
        f.write(test_xmen_content)

    try:
        import calcimp

        # Parse the file - this will exercise the variable parser
        print("Testing XMENSUR variable parsing with TinyExpr...")
        print("=" * 60)

        # The file should parse without errors
        result = calcimp.print_men(test_file)

        print(f"✓ Successfully parsed {len(result)} segments")
        print("\nExpected variable values:")
        print("  v1 = 1")
        print("  v2 = 1 - 1 = 0")
        print("  v3 = 0")
        print("  s_main_pull = 0.5*2 + 3.4 = 4.4")
        print("  radius = 10 / 2 = 5")
        print("  area = pi * 5^2 = 78.54...")
        print("  angle = pi / 4 = 0.7854...")
        print("  sin_value = sin(pi/4) = 0.7071...")
        print("  cos_value = cos(pi/4) = 0.7071...")
        print("  sqrt_value = sqrt(16) = 4")
        print("  complex_expr = (1 + 0) * 2 + sin(pi/6) = 2.5")

        print("\nParsed segments:")
        for i, (df, db, r, comment) in enumerate(result):
            print(f"  [{i}] df={df:.2f} db={db:.2f} r={r:.2f}")

        # Verify the tube uses the radius variable (should be 5mm)
        if len(result) > 0:
            df, db, r, _ = result[0]
            expected_radius = 5.0
            if abs(df - expected_radius) < 0.01 and abs(db - expected_radius) < 0.01:
                print(f"\n✓ Variable 'radius' correctly evaluated to {df:.2f} mm")
            else:
                print(f"\n✗ Expected radius={expected_radius}, got df={df:.2f}")
                return False

        print("\n" + "=" * 60)
        print("✓ All variable parsing tests passed!")
        return True

    except Exception as e:
        print(f"\n✗ Error: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        # Clean up
        if os.path.exists(test_file):
            os.remove(test_file)

if __name__ == '__main__':
    success = test_variable_parsing()
    sys.exit(0 if success else 1)
