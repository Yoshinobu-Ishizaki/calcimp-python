# calcimp-python

Python C extension module for calculating input impedance of wind instrument tubes using acoustic transmission theory.

管楽器の入力インピーダンスを音響伝送理論を用いて計算するPython C拡張モジュールです。

## 要件 (Requirements)

### System Dependencies
```bash
# Ubuntu/Debian
sudo apt-get install libglib2.0-dev libgsl-dev gcc python3-dev

# Fedora/RHEL
sudo dnf install glib2-devel gsl-devel gcc python3-devel
```

### Python Dependencies
- Python 3.13以上
- numpy
- matplotlib (for testing/visualization)

### External Library
- **Cephes Math Library** (`libmd.a`) - Required for Bessel and Struve functions
  - Must be built separately and placed in `../cephes_lib/` or custom path
  - Download from:
    - Original: http://www.netlib.org/cephes/
    - GitHub: https://github.com/jeremybarnes/cephes
    - GitHub (with build scripts): https://github.com/Yoshinobu-Ishizaki/cephes-lib

## ビルド方法 (Build Instructions)

### ⚠️ Important: CEPHES_PATH Environment Variable

**Before building, you MUST set the `CEPHES_PATH` environment variable** to point to your Cephes library directory containing `libmd.a`:

```bash
# Set CEPHES_PATH to your Cephes library location
export CEPHES_PATH=/path/to/your/cephes_lib

# Then build
uv run python setup.py build
```
Or use inline environment variable.

```bash
CEPHES_PATH=/path/to/cephes_lib uv run python setup.py build
```

### Expected Directory Structure
```
your-workspace/
├── calcimp-python/          # This project
│   ├── src/
│   ├── setup.py
│   └── ...
└── cephes_lib/              # Cephes library (if using default path)
    ├── libmd.a             # Required: Cephes static library
    ├── mconf.h             # Required: Configuration header
    └── ...
```

## インストール方法 (Installation)

### Development Mode (Recommended)
```bash
# After building
uv pip install -e .
```

### Regular Installation
```bash
uv run python setup.py install
```

## 使用例 (Usage Example)

```python
import calcimp
import numpy as np

# Calculate impedance for a tube geometry
frequencies, real_part, imag_part, magnitude_db = calcimp.calcimp(
    "sample/test.men",      # Mensur file (tube geometry)
    max_freq=2000.0,        # Maximum frequency in Hz
    step_freq=2.5,          # Frequency step in Hz
    temperature=24.0,        # Temperature in Celsius
    rad_calc=calcimp.PIPE,   # Type of radiation at the output end (PIPE/BUFFLE/NONE)
    dump_calc=True,         # Include dumping on the wall
    sec_var_calc=False      # Include effect by varying section area (experimental -- seems not adequate)
)

# Results are NumPy arrays
print(f"Calculated {len(frequencies)} frequency points")
print(f"Frequency range: {frequencies[0]:.1f} - {frequencies[-1]:.1f} Hz")
```

Parameters can be omitted to use default values explicitly written in the example above.

## テスト (Testing)

```bash
cd test
uv run python test_calcimp.py
```

The test will:
- Calculate impedance for `sample/test.men`
- Compare results with reference implementation
- Generate output file: `test/test_pycalcimp.imp`

## Mensur File Format

Mensur files (`.men`) describe tube geometry with CSV format:
```
# Comment line: tube description
df,db,length    # Front diameter, back diameter, length (all in mm)
10,10,1000      # Cylindrical tube: 10mm diameter, 1000mm length
10,0,0          # Terminator: open end
```

## トラブルシューティング (Troubleshooting)

### Error: "Cephes library not found"
```
RuntimeError: Cephes library not found at /path/to/libmd.a
Please set CEPHES_PATH environment variable or ensure libmd.a exists at ../cephes_lib/
```

**Solution:** Set `CEPHES_PATH` before building:
```bash
export CEPHES_PATH=/correct/path/to/cephes_lib
uv run python setup.py build
```

### Error: "multiple definition of ..."
**Solution:** Clean build directory and rebuild:
```bash
rm -rf build/ dist/ *.egg-info
uv run python setup.py build
```

### Crash when running
**Solution:** Ensure GSL (GNU Scientific Library) is installed:
```bash
sudo apt-get install libgsl-dev
```

## ライセンス (License)

Original code by Yoshinobu Ishizaki (1999)

## 参考文献 (References)

- Fletcher & Rossing: "The Physics of Musical Instruments"
- Cephes Mathematical Library: http://www.netlib.org/cephes/
