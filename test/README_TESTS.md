# Test Files

## Test Scripts

### test_format_parsing.py
Basic test to verify both XMENSUR (.xmen) and ZMENSUR (.men) formats can be parsed successfully.

**Run:**
```bash
cd test
python test_format_parsing.py
```

**Tests:**
1. Simple ZMENSUR format (../sample/test.men)
2. Simple XMENSUR format (../sample/test.xmen)
3. Complex XMENSUR with variables and groups (sample_xmensur.xmen)
4. Trumpet with valve XMENSUR (../sample/trumpet_valve.xmen)

### test_xmensur_equiv.py
Advanced test to compare XMENSUR and manually-created equivalent ZMENSUR files.

**Note:** Creating a perfect ZMENSUR equivalent of a complex XMENSUR file with nested groups is non-trivial. The XMENSUR parser handles nested `GROUP` blocks differently than simple ZMENSUR insertion directives.

**Status:** Work in progress - files parse but produce different results due to structural differences in group handling.

## Test Data Files

### sample_xmensur.xmen
Complex XMENSUR format example demonstrating:
- Variable definitions (`v1 = 1`, `s_main_pull = 0.5*2 + 3.4`)
- Nested GROUP blocks
- BRANCH/MERGE for valve routing
- MAIN/END_MAIN structure
- Comments with `#`

### sample_xmensur_equiv.men
Attempted ZMENSUR equivalent of sample_xmensur.xmen.

**Known issues:**
- Nested GROUP handling differs between formats
- Results don't match perfectly due to structural differences
- Manual conversion is complex and error-prone

**Recommendation:** Use XMENSUR format (.xmen) for complex instruments with nested structures and variables. Use ZMENSUR format (.men) for simpler, traditional mensur files.

## Running All Tests

```bash
cd test
python test_format_parsing.py
```

For original calcimp tests:
```bash
cd test
python test_calcimp.py
```
