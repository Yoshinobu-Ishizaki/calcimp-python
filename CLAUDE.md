# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**calcimp-python** is a Python C extension module for calculating the input impedance of wind instrument tubes using acoustic transmission theory. It combines a high-performance C physics engine (7000+ lines) with Python bindings for accessibility. Originally developed in 1999, it has been recently modernized with Python 3.13 support and Jupyter notebook integration.

### Source Code Attribution

The codebase consists of two main components:

**Original Implementation** (Yoshinobu Ishizaki, 1999):
- `calcimp.c/h` - Python extension interface
- `zmensur.c/h` - Physics engine and acoustic calculations
- `kutils.c/h` - Utility functions
- `xydata.c/h` - XY data structures
- `matutil.c/h` - Matrix operations

## Build & Development Commands

### Build from Source
```bash
# Install system dependencies first (Ubuntu/Debian)
sudo apt-get install libglib2.0-dev libgsl-dev

# Build the extension
uv python setup.py build

# Install in development mode
pip install -e .
```

### Testing
```bash
# Run unit tests and validation
cd test
# Compare with reference implementation
uv run python test_calcimp.py

# Interactive analysis
uv run upyter notebook test/compare_impedance.ipynb
```

### Using the Module
```python
import calcimp
import numpy as np

# Calculate impedance for a tube geometry
freq, real, imag, mag_db = calcimp.calcimp(
    "sample/test.men",      # Mensur file (tube geometry)
    max_freq=2000,          # Maximum frequency in Hz
    step_freq=2.5,          # Frequency step in Hz
    temperature=24.0        # Temperature in Celsius
)

# Returns tuple of 4 NumPy arrays:
# - frequencies (Hz)
# - real part of impedance
# - imaginary part of impedance
# - magnitude in dB
```

## Architecture Overview

### Three-Layer Design

1. **Python Extension Layer** (`src/calcimp.c`)
   - Exposes single function: `calcimp.calcimp()`
   - Handles NumPy array creation and Python/C data conversion
   - Calculates temperature-dependent acoustic constants

2. **Physics Engine** (`src/zmensur.c`, `src/zmensur.h`)
   - Core acoustic impedance calculations
   - Linked-list representation of tube segments
   - Transmission matrix method for cascaded tube sections
   - Physical effects: viscous wall friction, thermal losses, radiation impedance
   - Key functions:
     - `read_mensur()` - Parse tube geometry files
     - `input_impedance()` - Calculate complex impedance at a frequency
     - `transmission_matrix()` - Build 2x2 transfer matrix for each segment
     - `rad_imp()` - Calculate radiation impedance at open end

3. **Mathematical Functions** (Cephes library: `src/j0.c`, `src/j1.c`, `src/jv.c`, etc.)
   - Bessel functions (J0, J1, Jv, Yn) - ~2500 lines
   - Airy functions - 965 lines
   - Special functions (Gamma, Struve)
   - High-precision polynomial approximations

### Data Flow
```
Mensur file (.men)
  → Parse geometry (linked list of tube segments)
  → For each frequency:
      - Calculate transmission matrix per segment
      - Chain matrices via multiplication
      - Apply physical effects (friction, thermal, radiation)
      - Compute input impedance
  → Return NumPy arrays to Python
```

### Key Physical Constants

Defined in `src/zmensur.h` and `src/kutils.h`:
- `VRATIO = 3.3e-3` - Viscous loss coefficient (Fletcher & Rossing)
- `ARATIO = 6.0e-5` - Additional loss ratio
- `GMM = 1.4` - Heat capacity ratio (gamma for air)
- `Pr = 0.72` - Prandtl number
- `KIRCHHOFF_CONST = 0.004253682` - Kirchhoff damping constant

Temperature-dependent variables calculated at runtime:
- `c0` - Speed of sound: 331.45 × √(T/273.16 + 1)
- `rho` - Air density: 1.2929 × (273.16/(273.16 + T))
- `mu` - Dynamic viscosity: (18.2 + 0.0456×(T-25)) × 10⁻⁶
- `nu` - Kinematic viscosity: mu/rho

## File Formats

### Mensur Files (.men)
Describe tube geometry with line format: `df,db,length`
- `df` - Front diameter (mm)
- `db` - Back diameter (mm)
- `length` - Length (mm)

Example (`sample/test.men`):
```
sample test mensur
10,10,1000    # Cylindrical tube: 10mm diameter, 1000mm length
10,0,0        # Open end terminator
```

Special markers (defined in `src/kutils.h`):
- `%` - Comment line
- `-` - Tone hole
- `>` - Branch/split
- `<` - Join

### Impedance Files (.imp)
CSV format output: `freq,imp.real,imp.imag,mag`
```
0.000000,0.0000000000E+00,0.0000000000E+00,0.0000000000E+00
2.500000,1.2345678901E+03,2.3456789012E+02,1.2345678901E+01
```

## Code Organization Patterns

### Mensur Structure (Linked List)
```c
struct men_s {
    double df, db, r;           // Front/back diameter, length
    char comment[64];            // Metadata
    struct men_s *prev, *next;   // Chain segments
    struct men_s *side;          // Side branches (tone holes)
    double complex zi, zo;       // Input/output impedance
    double complex m11, m12, m21, m22;  // 2x2 transmission matrix
    double complex ui, uo;       // Particle velocity
};
```

### Transmission Matrix Method
Each tube segment contributes a 2x2 complex matrix. Total system matrix is:
```
M_total = M_1 × M_2 × M_3 × ... × M_n
```
Input impedance derived from final matrix elements.

### Global Configuration Flags
- `RAD_CALC` - Radiation model: NONE, PIPE, BUFFLE
- `DUMP_CALC` - Viscous damping: NONE, WALL

### Memory Management
Custom error-checked allocators in `src/kutils.c`:
- `m_malloc()` - Error-checked malloc
- `m_calloc()` - Error-checked calloc
- `m_realloc()` - Error-checked realloc

## Important Implementation Details

### Build System
- Uses `pkg-config` to locate glib-2.0 and GSL dependencies
- Compilation flag: `-O3` for optimization
- NumPy C API: Uses `NPY_NO_DEPRECATED_API` with NumPy 1.7 API version
- All C source files must be listed explicitly in `setup.py:get_sources()`

### Python/C Interface
- Single exposed function: `py_calcimp()` in `src/calcimp.c`
- Returns tuple of 4 NumPy arrays (frequencies, real, imag, magnitude_dB)
- NumPy arrays created using `PyArray_SimpleNewFromData()`
- Memory allocated with C malloc, ownership transferred to Python

### Testing Approach
Tests compare Python implementation output against reference C implementation:
- Generate: `sample/test_pycalcimp.imp`
- Compare with: `sample/test.imp`
- Tolerance: ~10% relative difference (due to floating-point variations)
- Look for resonance peaks (maxima in magnitude) to validate physics

### Jupyter Notebook Usage
The `test/compare_impedance.ipynb` notebook:
- Loads both reference and Python implementation outputs
- Plots impedance vs. frequency with multiple visualizations
- Analyzes peak regions (resonances)
- Calculates statistical differences
- Recent conversion: Uses `polars` instead of `pandas` for data handling

## Development Notes

### Language Mix
- Core computation: C (for performance)
- User interface: Python (for accessibility)
- Documentation: Mixed Japanese/English (original 1999 codebase)

### Historical Context
- Original author: Yoshinobu Ishizaki (since 1999)
- Based on Fletcher & Rossing acoustic theory
- Originally standalone CLI, recently converted to Python library
- Cephes math library: Copyright Stephen L. Moshier (1995)

### System Dependencies
Required packages (Ubuntu/Debian):
```bash
sudo apt-get install libglib2.0-dev libgsl-dev gcc python3-dev
```

### Python Environment
- Minimum Python version: 3.13 (specified in `pyproject.toml`)
- Virtual environment present: `.venv/`
- Package manager: Uses `uv` (lockfile: `uv.lock`)
- Required Python packages: numpy, matplotlib>=3.10.7, ipykernel>=7.0.1

## Common Patterns When Editing

### Adding New Mensur Parameters
1. Modify `struct men_s` in `src/zmensur.h`
2. Update parsing in `read_mensur()` in `src/zmensur.c`
3. Update transmission matrix calculation if needed
4. Test with modified `.men` file

### Adding Physical Effects
1. Add constants to `src/zmensur.h` or `src/kutils.h`
2. Modify `transmission_matrix()` or `input_impedance()` in `src/zmensur.c`
3. Update temperature-dependent calculations in `src/calcimp.c` if needed
4. Validate against known physical models

### Modifying Python Interface
1. Edit `py_calcimp()` in `src/calcimp.c`
2. Update parameter parsing and NumPy array creation
3. Test with `test/test_calcimp.py`
4. Update notebooks if interface changes

### Debugging
- C level: Add printf statements (results appear in Python)
- Python level: Standard Python debugging tools
- Check mensur parsing: Examine linked-list structure
- Validate matrices: Enable debug output in transmission calculations

## Current Status

### Recent Development
- Successfully compiled with Python 3.13
- Test suite passing with reference comparison
- Jupyter notebooks working with polars for data analysis
- Git status: Modified `pyproject.toml` and `uv.lock` (uncommitted)

### Known Limitations
- Limited English documentation (mostly Japanese comments)
- Single reference test case (`sample/test.men`)
- No CI/CD pipeline configured
- Package naming inconsistency: `calcimp-python` in pyproject.toml vs. `calcimp` in setup.py
