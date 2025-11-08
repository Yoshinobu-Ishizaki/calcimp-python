# calcimp-python

Python C extension module for calculating input impedance of wind instrument tubes using acoustic transmission theory.

管楽器の入力インピーダンスを音響伝送理論を用いて計算するPython C拡張モジュールです。

## 要件 (Requirements)

### System Dependencies

**Required for all installation methods (including pre-built wheels):**

```bash
# Ubuntu/Debian
sudo apt-get install libglib2.0-0 libgsl27

# For building from source, also install development packages:
sudo apt-get install libglib2.0-dev libgsl-dev gcc python3-dev

# macOS
brew install glib gsl

# Windows (MSYS2)
pacman -S mingw-w64-x86_64-glib2 mingw-w64-x86_64-gsl
```

### Python Dependencies
- Python 3.13以上
- numpy
- matplotlib (for testing/visualization)

### External Library
- **Cephes Math Library** (`libmd.a`) - Required for Bessel and Struve functions
  - **Automatically downloaded and built during installation** when installing from git
  - For manual builds, can be placed in `../cephes_lib/` or custom path via `CEPHES_PATH`
  - Repository: https://github.com/Yoshinobu-Ishizaki/cephes-lib

## ビルド方法 (Build Instructions)

### Build Dependencies

When building from source (not needed for pre-built wheels), you'll need:

```bash
# Ubuntu/Debian - build tools for cephes-lib
sudo apt-get install git autoconf automake libtool make gcc

# macOS
brew install autoconf automake libtool
```

The Cephes library will be **automatically downloaded and built** during installation. No manual setup required!

### Optional: CEPHES_PATH Environment Variable

If you already have Cephes library built elsewhere, you can point to it:

```bash
# Set CEPHES_PATH to your existing Cephes library location
export CEPHES_PATH=/path/to/your/cephes_lib

# Then build
uv run python setup.py build
```

The build process will search for cephes-lib in this order:
1. `CEPHES_PATH` environment variable (if set)
2. `../cephes-lib` (for local development)
3. `../cephes_lib` (alternative naming)
4. `.cephes-lib` (auto-downloaded during pip install)

## インストール方法 (Installation)

### Option 1: Install Directly from Git (Easiest - Auto-builds)

Install directly from GitHub repository. This will automatically download and build all dependencies including cephes-lib:

```bash
# Install from main branch
pip install git+https://github.com/Yoshinobu-Ishizaki/calcimp-python.git

# Or using uv
uv pip install git+https://github.com/Yoshinobu-Ishizaki/calcimp-python.git

# Install specific branch or tag
pip install git+https://github.com/Yoshinobu-Ishizaki/calcimp-python.git@dev
pip install git+https://github.com/Yoshinobu-Ishizaki/calcimp-python.git@v0.2.0
```

**Prerequisites:**
- System dependencies (glib2, gsl) must be installed (see System Dependencies section)
- Build tools (git, autoconf, automake, libtool, make, gcc) - see Build Dependencies section

The cephes-lib will be automatically cloned and built during installation!

### Option 2: Install Pre-built Wheel from GitHub Releases

Pre-built wheel packages are available for Ubuntu, macOS, and Windows from GitHub Releases:

**Prerequisites:**
- System dependencies (glib2, gsl) must still be installed (see System Dependencies section)

**Steps:**

1. **Visit the Releases page:**
   https://github.com/Yoshinobu-Ishizaki/calcimp-python/releases

2. **Download the wheel for your platform:**
   - **Ubuntu/Linux**: `calcimp_python-*-linux_*.whl`
   - **macOS**: `calcimp_python-*-macosx_*.whl`
   - **Windows**: `calcimp_python-*-win_*.whl`

3. **Install the wheel:**
```bash
# Using pip
pip install calcimp_python-*.whl

# Or using uv
uv pip install calcimp_python-*.whl
```

**Alternative: Using GitHub CLI**

If you have GitHub CLI installed (https://cli.github.com/):

```bash
# List available releases
gh release list --repo Yoshinobu-Ishizaki/calcimp-python

# Download latest release wheels
gh release download --repo Yoshinobu-Ishizaki/calcimp-python --pattern "*.whl"

# Install
pip install calcimp_python-*.whl
```

**Note for Development Builds:** GitHub Actions artifacts from recent commits are also available but expire after 90 days. See the Actions tab for development builds.

### Option 3: Build from Source (Local Development)

For local development, clone the repository and build:

```bash
# Clone the repository
git clone https://github.com/Yoshinobu-Ishizaki/calcimp-python.git
cd calcimp-python

# Build and install in development mode (recommended)
pip install -e .

# Or using uv
uv pip install -e .
```

The build process will automatically handle cephes-lib download and compilation if needed.

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

## Mensur File Formats

calcimp supports two file formats for describing tube geometry:

### 1. ZMENSUR Format (.men)

CSV-based format with `%` comments:

```
# Basic tube
10,10,1000,     % Cylindrical tube: 10mm diameter, 1000mm length
10,0,0,         % Open end terminator
```

**Extended features:**
- Variable definitions and reusable sub-mensur sections
- Tone holes with adjustable openness (`-name,ratio`)
- Valve branches and detour routing (`>name,ratio` / `<name,ratio`)

**[See doc/zmensur.md](doc/zmensur.md)** for complete ZMENSUR format specification.

### 2. XMENSUR Format (.xmen)

Python-style format with `#` comments, brackets, and expressions:

```
# Trumpet with valve
bore_dia = 11.5  # Variable definition

[  # Main bore
    10, bore_dia, 100,  # Can use expressions
    >, VALVE1, 1,       # Valve branch
    bore_dia, bore_dia, 150,
    <, VALVE1, 1,       # Valve merge
    OPEN_END
]

{, VALVE1  # Valve slide definition
    bore_dia, bore_dia, 300,
    OPEN_END
}
```

**Key differences:**
- Uses `#` for comments (like Python)
- Supports Python arithmetic expressions and variables
- Uses brackets `[]` and `{}` for grouping
- Keywords: `OPEN_END`, `CLOSED_END`, `MAIN`, `GROUP`
- Aliases: `>` = BRANCH, `<` = MERGE, `|` = SPLIT

**[See doc/xmensur.md](doc/xmensur.md)** for complete XMENSUR format specification.

### Usage

Both formats work seamlessly - just use the appropriate file extension:

```python
import calcimp

# ZMENSUR format
freq, real, imag, mag = calcimp.calcimp("instrument.men")

# XMENSUR format (automatically converted)
freq, real, imag, mag = calcimp.calcimp("instrument.xmen")
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

## 開発者向け (For Developers)

### Creating a Release

To create a new release with pre-built wheels:

1. **Create and push a version tag:**
```bash
git tag v0.1.0
git push origin v0.1.0
```

2. **GitHub Actions will automatically:**
   - Build wheels for Ubuntu, macOS, and Windows
   - Run tests on all platforms
   - Create a GitHub Release
   - Upload wheels to the release

3. **Tag naming convention:**
   - Use semantic versioning: `v<major>.<minor>.<patch>`
   - Examples: `v0.1.0`, `v1.0.0`, `v1.2.3`

## ライセンス (License)

Original code by Yoshinobu Ishizaki (1999)

## Acknowledgements

This project incorporates the following third-party libraries:

- **Cephes Mathematical Library** by Stephen L. Moshier
  - Provides special mathematical functions (Bessel, Struve, Airy functions)
  - Copyright (C) 1995 Stephen L. Moshier
  - http://www.netlib.org/cephes/

- **TinyExpr** by Lewis Van Winkle
  - Lightweight expression parser for mathematical expressions in XMENSUR files
  - Copyright (c) 2015-2020 Lewis Van Winkle
  - https://github.com/codeplea/tinyexpr
  - Licensed under the zlib license

## 参考文献 (References)

- Fletcher & Rossing: "The Physics of Musical Instruments"
