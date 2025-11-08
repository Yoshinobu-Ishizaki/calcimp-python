"""
Python wrapper for calcimp that adds XMENSUR support.

This module provides a high-level interface to the calcimp C extension
that automatically handles both ZMENSUR (.men) and XMENSUR (.xmen) file formats.
"""

import os
import tempfile
from . import _calcimp_c


def calcimp(filename, max_freq=2000.0, step_freq=2.5, num_freq=0, temperature=24.0,
            rad_calc=None, dump_calc=True, sec_var_calc=False):
    """Calculate input impedance of a tube.

    This function supports both ZMENSUR (.men) and XMENSUR (.xmen) file formats.
    XMENSUR files are automatically converted to ZMENSUR format before processing.

    Parameters:
        filename (str): Path to the mensur file (.men or .xmen)
        max_freq (float, optional): Maximum frequency in Hz (default: 2000.0)
        step_freq (float, optional): Frequency step in Hz (default: 2.5)
        num_freq (int, optional): Number of frequency points (overrides step_freq if > 0)
        temperature (float, optional): Temperature in Celsius (default: 24.0)
        rad_calc (int, optional): Radiation impedance mode - calcimp.PIPE, calcimp.BUFFLE,
                                  or calcimp.NONE (default: calcimp.PIPE)
        dump_calc (bool, optional): Enable wall damping calculation (default: True)
        sec_var_calc (bool, optional): Enable section variation calculation (default: False)

    Returns:
        tuple: (frequencies, real_part, imaginary_part, magnitude_db)
               All return values are NumPy arrays.

    Examples:
        >>> import calcimp
        >>> freq, real, imag, mag_db = calcimp.calcimp("sample.men")
        >>> freq, real, imag, mag_db = calcimp.calcimp("sample.xmen")  # XMENSUR format
    """
    # Default rad_calc to PIPE if not specified
    if rad_calc is None:
        rad_calc = _calcimp_c.PIPE

    # Check file extension
    _, ext = os.path.splitext(filename)
    ext = ext.lower()

    # If it's an XMENSUR file, convert it first
    if ext == '.xmen':
        # Import xmensur module
        try:
            from . import xmensur
        except ImportError:
            raise ImportError(
                "XMENSUR support requires the xmensur module. "
                "Please ensure xmensur.py is properly installed."
            )

        # Create a temporary file for the converted zmensur
        with tempfile.NamedTemporaryFile(mode='w', suffix='.men', delete=False) as tmp:
            tmp_path = tmp.name

        try:
            # Convert XMENSUR to ZMENSUR
            xmensur.xmensur_to_zmensur_file(filename, tmp_path)

            # Calculate using the converted file
            result = _calcimp_c.calcimp(
                tmp_path, max_freq, step_freq, num_freq, temperature,
                rad_calc, dump_calc, sec_var_calc
            )

            return result

        finally:
            # Clean up temporary file
            if os.path.exists(tmp_path):
                os.unlink(tmp_path)

    else:
        # It's a regular ZMENSUR file (.men) or unknown extension
        # Pass directly to C extension
        return _calcimp_c.calcimp(
            filename, max_freq, step_freq, num_freq, temperature,
            rad_calc, dump_calc, sec_var_calc
        )


# Re-export constants from C extension
NONE = _calcimp_c.NONE
PIPE = _calcimp_c.PIPE
BUFFLE = _calcimp_c.BUFFLE


# Provide access to the xmensur module if needed
def convert_xmensur(xmen_path, zmen_path=None):
    """Convert an XMENSUR file to ZMENSUR format.

    This is a convenience function that exposes the xmensur converter directly.

    Args:
        xmen_path (str): Path to input .xmen file
        zmen_path (str, optional): Path to output .men file
                                   (defaults to same name with .men extension)

    Returns:
        str: Path to the generated zmensur file

    Example:
        >>> import calcimp
        >>> output_path = calcimp.convert_xmensur("instrument.xmen", "instrument.men")
    """
    try:
        from . import xmensur
    except ImportError:
        raise ImportError(
            "XMENSUR support requires the xmensur module. "
            "Please ensure xmensur.py is properly installed."
        )

    return xmensur.xmensur_to_zmensur_file(xmen_path, zmen_path)
