"""
Python wrapper for calcimp.

This module provides a high-level interface to the calcimp C extension
that automatically handles both ZMENSUR (.men) and XMENSUR (.xmen) file formats.
"""

from . import _calcimp_c


def calcimp(filename, max_freq=2000.0, step_freq=2.5, num_freq=0, temperature=24.0,
            rad_calc=None, dump_calc=True, sec_var_calc=False):
    """Calculate input impedance of a tube.

    This function supports both ZMENSUR (.men) and XMENSUR (.xmen) file formats.
    The C extension automatically detects the format based on file extension.

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

    # Pass directly to C extension - it handles both .men and .xmen formats
    return _calcimp_c.calcimp(
        filename, max_freq, step_freq, num_freq, temperature,
        rad_calc, dump_calc, sec_var_calc
    )


# Re-export constants from C extension
NONE = _calcimp_c.NONE
PIPE = _calcimp_c.PIPE
BUFFLE = _calcimp_c.BUFFLE
