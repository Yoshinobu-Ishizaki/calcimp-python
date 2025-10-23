from setuptools import setup, Extension
import numpy as np
import os

def get_sources():
    sources = [
        'src/calcimp.c',
        'src/kutils.c',
        'src/zmensur.c',
    ]
    return sources

module = Extension('calcimp',
                  sources=get_sources(),
                  include_dirs=[np.get_include(), 'src'],
                  extra_compile_args=['-O3'],
                  define_macros=[('NPY_NO_DEPRECATED_API', 'NPY_1_7_API_VERSION')])

setup(name='calcimp',
      version='0.1',
      description='Calculate input impedance of tubes',
      ext_modules=[module],
      python_requires='>=3.6',
      install_requires=['numpy']
      )
