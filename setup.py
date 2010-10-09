import os
from subprocess import Popen
from distutils.core import setup, Extension

Popen('./.make-http-parser').wait()

C_FILES = [os.path.join('bjoern', f) for f in os.listdir('bjoern') if f.endswith('.c')]
CFLAGS = '-std=c99 -Wall -Wextra -Wno-unused -fno-strict-aliasing -shared -O3 '

setup(
    name        = 'bjoern',
    description = 'A screamingly fast Python WSGI server written in C.',
    version     = '0.2',
    package_dir = {'': 'bjoern'},
    ext_modules = [
        Extension('bjoern',
            sources       = C_FILES + ['include/http-parser/http_parser.c'],
            include_dirs  = ['include/http-parser', '.'],
            libraries     = ['ev'],
            define_macros = [('WANT_ROUTING', True), ('WANT_SENDFILE', os.name == 'posix')],
            extra_link_args = ['-static', '-g'],
            extra_compile_args = CFLAGS.split()
        )
    ]
)
