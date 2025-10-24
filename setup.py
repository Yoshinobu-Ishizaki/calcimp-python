from setuptools import setup, Extension
import numpy as np
import os
import subprocess

def pkg_config(*packages, **kw):
    """Get compiler and linker flags from pkg-config."""
    flag_map = {'-I': 'include_dirs', '-L': 'library_dirs', '-l': 'libraries'}

    for package in packages:
        # Run pkg-config
        pkg_config_cmd = ['pkg-config', '--cflags', '--libs', package]
        try:
            tokens = subprocess.check_output(pkg_config_cmd).decode().split()
        except subprocess.CalledProcessError:
            print(f"Warning: pkg-config failed for {package}")
            continue

        # Sort flags into appropriate categories
        for token in tokens:
            if token[:2] in flag_map:
                kw.setdefault(flag_map[token[:2]], []).append(token[2:])
            else:
                kw.setdefault('extra_compile_args', []).append(token)
    return kw

def get_cephes_config():
    """Get Cephes library path and configuration."""
    cephes_path = os.environ.get('CEPHES_PATH', '../cephes_lib')
    cephes_lib = os.path.join(cephes_path, 'libmd.a')

    # Convert to absolute path for reliable linking
    cephes_path = os.path.abspath(cephes_path)
    cephes_lib = os.path.abspath(cephes_lib)

    if not os.path.exists(cephes_lib):
        raise RuntimeError(
            f"Cephes library not found at {cephes_lib}\n"
            f"Please set CEPHES_PATH environment variable or ensure libmd.a exists at ../cephes_lib/"
        )

    print(f"Using Cephes library: {cephes_lib}")

    # Use extra_objects for static library linking (standard method)
    return {
        'include_dirs': [cephes_path],
        'extra_objects': [cephes_lib]
    }

def get_sources():
    """Get list of source files (excluding Cephes sources - linked via libmd.a)."""
    sources = [
        'src/calcimp.c',
        'src/acoustic_constants.c',
        'src/kutils.c',
        'src/zmensur.c',
        'src/xydata.c',
        'src/matutil.c',
    ]
    return sources

# Get compiler and linker flags for glib-2.0 and gsl
ext_kwargs = pkg_config('glib-2.0', 'gsl')

# Get Cephes library configuration
cephes_config = get_cephes_config()

module = Extension('calcimp',
                  sources=get_sources(),
                  include_dirs=[np.get_include(), 'src'] +
                               ext_kwargs.get('include_dirs', []) +
                               cephes_config['include_dirs'],
                  libraries=ext_kwargs.get('libraries', []) + ['m'],  # Add libm for math functions
                  library_dirs=ext_kwargs.get('library_dirs', []),
                  extra_compile_args=['-O3'] + ext_kwargs.get('extra_compile_args', []),
                  extra_objects=cephes_config['extra_objects'],  # Static library (.a file)
                  extra_link_args=ext_kwargs.get('extra_link_args', []),
                  define_macros=[('NPY_NO_DEPRECATED_API', 'NPY_1_7_API_VERSION')])

setup(name='calcimp',
      version='0.1',
      description='Calculate input impedance of tubes',
      ext_modules=[module],
      python_requires='>=3.6',
      install_requires=['numpy']
      )
