import os
from subprocess import Popen
from distutils.core import setup, Extension
from distutils.sysconfig import get_python_inc

Popen('stuff/update-http-parser.sh').wait()
C_FILES = [os.path.join('src', f) for f in os.listdir('src') if f.endswith('.c')]
CFLAGS = '-std=c99 -Wall -fno-strict-aliasing -shared -O3 '

setup(
    name        = 'bjoern',
    description = 'A screamingly fast Python WSGI server written in C.',
    version     = '0.2',
    package_dir = {'': 'src'},
    py_modules  = ['bjoern'],
    ext_modules = [
        Extension('_bjoern',
            sources       = C_FILES + ['include/http-parser/http_parser.c'],
            include_dirs  = ['include/http-parser', '.'],
            libraries     = ['ev'],
            define_macros = [('WANT_ROUTING', True), ('WANT_SENDFILE', os.name == 'posix')],
            extra_link_args = ['-static', '-g'],
            extra_compile_args = CFLAGS.split()
        )
    ]
)

