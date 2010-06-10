import os
from distutils.core import setup, Extension
from distutils.sysconfig import get_python_inc

if 'CFLAGS' not in os.environ:
    os.environ['CFLAGS'] = ''
os.environ['CFLAGS'] += '-std=c99 -pedantic -Wall -fno-strict-aliasing -shared -O3'

setup(name='bjoern',
      version='0.1',
      description="lala",
      package_dir={'': 'src'},
      py_modules=['bjoern'],
      ext_modules = [
          Extension('_bjoern',
                  sources=['src/bjoern.c'],
                  include_dirs=[get_python_inc(plat_specific=1), 'include/http-parser', '.'],
                  library_dirs=[],
                  libraries=[],
                  define_macros=[('WANT_ROUTING',)],
                  )
          ]
)

