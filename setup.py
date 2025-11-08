from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import numpy as np
import os
import subprocess
import platform
import shutil
import sys

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

class CustomBuildExt(build_ext):
    """Custom build_ext command that downloads and builds cephes-lib if needed."""

    def run(self):
        """Override run to build cephes-lib before building extensions."""
        # Build cephes-lib if needed
        self.build_cephes_lib()

        # Update extension configuration with cephes library paths
        cephes_config = self.get_cephes_config()
        for ext in self.extensions:
            if ext.name == 'calcimp._calcimp_c':
                ext.include_dirs.extend(cephes_config['include_dirs'])
                ext.extra_objects.extend(cephes_config['extra_objects'])

        # Continue with normal extension build
        build_ext.run(self)

    def build_cephes_lib(self):
        """Download and build cephes-lib if not already available."""
        # Determine cephes path
        cephes_path = os.environ.get('CEPHES_PATH')

        if cephes_path is None:
            # First, try ../cephes-lib (for local development)
            parent_cephes = os.path.abspath('../cephes-lib')
            if os.path.exists(parent_cephes):
                cephes_path = parent_cephes
            else:
                # Try ../cephes_lib (underscore)
                parent_cephes_underscore = os.path.abspath('../cephes_lib')
                if os.path.exists(parent_cephes_underscore):
                    cephes_path = parent_cephes_underscore
                else:
                    # Use local directory for pip install from git
                    # This will be in the source directory during build
                    cephes_path = os.path.abspath('.cephes-lib')
        else:
            cephes_path = os.path.abspath(cephes_path)

        cephes_lib = os.path.join(cephes_path, 'libmd.a')

        # Check if library already exists
        if os.path.exists(cephes_lib):
            print(f"Found existing Cephes library at {cephes_lib}")
            return

        print(f"Cephes library not found at {cephes_lib}")
        print("Downloading and building cephes-lib...")

        # Clone the repository if directory doesn't exist
        if not os.path.exists(cephes_path):
            parent_dir = os.path.dirname(cephes_path)
            if not os.path.exists(parent_dir):
                os.makedirs(parent_dir)

            print(f"Cloning cephes-lib to {cephes_path}...")
            try:
                subprocess.check_call([
                    'git', 'clone',
                    'https://github.com/Yoshinobu-Ishizaki/cephes-lib.git',
                    cephes_path
                ])
            except subprocess.CalledProcessError as e:
                raise RuntimeError(
                    f"Failed to clone cephes-lib repository: {e}\n"
                    "Please ensure git is installed and you have internet access."
                ) from e

        # Build the library
        print(f"Building cephes-lib in {cephes_path}...")
        try:
            # Run autoreconf if configure doesn't exist
            configure_path = os.path.join(cephes_path, 'configure')
            if not os.path.exists(configure_path):
                print("Running autoreconf to generate configure script...")
                subprocess.check_call(
                    ['autoreconf', '-i'],
                    cwd=cephes_path
                )

            # Run configure if Makefile doesn't exist
            makefile_path = os.path.join(cephes_path, 'Makefile')
            if not os.path.exists(makefile_path):
                print("Running configure...")
                subprocess.check_call(
                    ['./configure'],
                    cwd=cephes_path
                )

            # Build the library
            print("Running make...")
            subprocess.check_call(
                ['make'],
                cwd=cephes_path
            )

            # Verify the library was built
            if not os.path.exists(cephes_lib):
                raise RuntimeError(
                    f"Build succeeded but library not found at {cephes_lib}"
                )

            print(f"Successfully built Cephes library at {cephes_lib}")

        except subprocess.CalledProcessError as e:
            raise RuntimeError(
                f"Failed to build cephes-lib: {e}\n"
                "Please check build logs for details."
            ) from e
        except FileNotFoundError as e:
            raise RuntimeError(
                f"Build tool not found: {e}\n"
                "Please ensure autotools and make are installed:\n"
                "  Ubuntu/Debian: sudo apt-get install autoconf automake libtool make\n"
                "  macOS: brew install autoconf automake libtool"
            ) from e

    def get_cephes_config(self):
        """Get Cephes library path and configuration."""
        cephes_path = os.environ.get('CEPHES_PATH')

        if cephes_path is None:
            # First, try ../cephes-lib (for local development)
            parent_cephes = os.path.abspath('../cephes-lib')
            if os.path.exists(parent_cephes):
                cephes_path = parent_cephes
            else:
                # Try ../cephes_lib (underscore)
                parent_cephes_underscore = os.path.abspath('../cephes_lib')
                if os.path.exists(parent_cephes_underscore):
                    cephes_path = parent_cephes_underscore
                else:
                    # Use local directory for pip install from git
                    cephes_path = os.path.abspath('.cephes-lib')
        else:
            cephes_path = os.path.abspath(cephes_path)

        cephes_lib = os.path.join(cephes_path, 'libmd.a')

        # Convert to absolute path for reliable linking
        cephes_path = os.path.abspath(cephes_path)
        cephes_lib = os.path.abspath(cephes_lib)

        if not os.path.exists(cephes_lib):
            raise RuntimeError(
                f"Cephes library not found at {cephes_lib}\n"
                f"This should have been built automatically. Please check build logs."
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
        'src/xmensur.c',
        'src/xydata.c',
        'src/matutil.c',
    ]
    return sources

# Get compiler and linker flags for glib-2.0 and gsl
ext_kwargs = pkg_config('glib-2.0', 'gsl')

# Platform-specific linker flags
extra_link_args = ext_kwargs.get('extra_link_args', [])
if platform.system() == 'Windows':
    # Allow multiple definitions to resolve conflicts between cephes and MSVCRT Bessel functions
    extra_link_args.append('-Wl,--allow-multiple-definition')

# Create extension without cephes configuration (will be added during build)
# Extension module is named calcimp._calcimp_c to avoid name collision with package
module = Extension('calcimp._calcimp_c',
                  sources=get_sources(),
                  include_dirs=[np.get_include(), 'src'] +
                               ext_kwargs.get('include_dirs', []),
                  libraries=ext_kwargs.get('libraries', []) + ['m'],  # Add libm for math functions
                  library_dirs=ext_kwargs.get('library_dirs', []),
                  extra_compile_args=['-O3'] + ext_kwargs.get('extra_compile_args', []),
                  extra_objects=[],  # Will be populated during build with cephes library
                  extra_link_args=extra_link_args,
                  define_macros=[('NPY_NO_DEPRECATED_API', 'NPY_1_7_API_VERSION')])

setup(name='calcimp',
      version='0.4.0',
      description='Calculate input impedance of tubes',
      ext_modules=[module],
      packages=['calcimp'],
      package_dir={'calcimp': 'calcimp'},
      cmdclass={'build_ext': CustomBuildExt},
      python_requires='>=3.6',
      install_requires=['numpy']
      )
