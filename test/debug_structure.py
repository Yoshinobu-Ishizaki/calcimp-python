#!/usr/bin/env python3
"""
Debug script to check mensur structure before rejoint_men fails
"""

import sys

# Add temporary debug output to xmensur.c before rejoint_men
# Then this script can analyze the structure

print("To debug the 'Cannot find joining point' error:")
print()
print("The issue is in resolve_xmen_child at line 557:")
print("  if ( m->s_type != JOIN)")
print("    m->side = child;")
print()
print("This means JOIN markers do NOT get side attached.")
print("But get_join_men() expects JOIN markers to have side set.")
print()
print("Solution: Both SPLIT and JOIN markers need side attached.")
print("Remove the 'if ( m->s_type != JOIN)' condition.")
print()
print("Also fix lines 560-561 - the else block logic is wrong.")
print("If child is NULL, we shouldn't be in that if block at all.")
