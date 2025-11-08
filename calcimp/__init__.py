"""
calcimp - Calculate input impedance of wind instrument tubes

This package provides acoustic impedance calculations for wind instrument bore geometries
using transmission line theory. It supports both ZMENSUR (.men) and XMENSUR (.xmen) file formats.

Main function:
    calcimp(filename, ...) - Calculate input impedance from a mensur file

Constants:
    NONE   - No radiation impedance calculation
    PIPE   - Pipe radiation impedance (default)
    BUFFLE - Infinite baffle radiation impedance

Example:
    >>> import calcimp
    >>> freq, real, imag, mag_db = calcimp.calcimp("sample.men")
    >>> # Or with XMENSUR format:
    >>> freq, real, imag, mag_db = calcimp.calcimp("sample.xmen")
"""

# Import the C extension module
from . import _calcimp_c

# Import the Python wrapper
from .calcimp_wrapper import calcimp

# Re-export constants
NONE = _calcimp_c.NONE
PIPE = _calcimp_c.PIPE
BUFFLE = _calcimp_c.BUFFLE

# Define public API
__all__ = [
    'calcimp',
    'NONE',
    'PIPE',
    'BUFFLE',
]

__version__ = '0.4.0'
