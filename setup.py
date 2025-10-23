from setuptools import setup, Extension

module = Extension('calcimp',
                  sources=['src/calcimp.c'])

setup(name='calcimp',
      version='0.1',
      description='A Python C extension example',
      ext_modules=[module],
      python_requires='>=3.6'
      )
