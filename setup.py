import os
from distutils.core import setup, Extension

SOURCE_FILES = [
    'http-parser/http_parser.c', 'bjoern/request.c', 'bjoern/bjoernmodule.c',
    'bjoern/server.c', 'bjoern/wsgi.c'
]

setup(
    name         = 'bjoern',
    author       = 'Jonas Haag',
    author_email = 'jonas@lophus.org',
    license      = '2-clause BSD',
    url          = 'https://github.com/jonashaag/bjoern',
    description  = 'A screamingly fast Python WSGI server written in C.',
    version      = '1.0',
    classifiers  = ['Development Status :: 4 - Beta',
                    'License :: OSI Approved :: BSD License',
                    'Programming Language :: C',
                    'Topic :: Internet :: WWW/HTTP :: WSGI :: Server'],
    ext_modules  = [
        Extension(
            'bjoern',
            sources=SOURCE_FILES,
            libraries=['ev'],
            include_dirs=['http-parser'],
            define_macros=[('WANT_SENDFILE', '1'), ('WANT_SIGINT_HANDLING', '1')],
            extra_compile_args='-std=c99 -fno-strict-aliasing -Wall -Wextra -Wno-unused -g -fPIC'.split()
        )
    ]
)
