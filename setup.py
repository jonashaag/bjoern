# -*- coding: utf-8 -*-

import os.path
from distutils.core import setup, Extension

SOURCE_FILES = [
    os.path.join('http-parser', 'http_parser.c'),
    os.path.join('bjoern', 'request.c'),
    os.path.join('bjoern', 'bjoernmodule.c'),
    os.path.join('bjoern', 'server.c'),
    os.path.join('bjoern', 'wsgi.c'),
]

ext_modules = [
    Extension(
        'bjoern',
        sources=SOURCE_FILES,
        libraries=['ev'],
        include_dirs=['http-parser'],
        define_macros=[('WANT_SENDFILE', '1'),
                       ('WANT_SIGINT_HANDLING', '1')],
        extra_compile_args=[
            '-std=c99',
            '-fno-strict-aliasing',
            '-Wall',
            '-Wextra',
            '-Wno-unused',
            '-g',
            '-fPIC'
            ]
        )
]

def read_file_contents(filename):
    """
    Reads the contents of a given file relative to the directory
    containing this file and returns it.

    :param filename:
        The file to open and read contents from.
    """
    return open(os.path.join(os.path.dirname(__file__), filename)).read()

setup(
    name='bjoern',
    author='Jonas Haag',
    author_email='jonas@lophus.org',
    license='2-clause BSD',
    url='https://github.com/jonashaag/bjoern',
    description='A screamingly fast Python WSGI server written in C.',
    long_description=read_file_contents('README'),
    version='1.0',
    classifiers=[
        'Development Status :: 4 - Beta',
        'License :: OSI Approved :: BSD License',
        'Programming Language :: C',
        'Programming Language :: Python',
        'Topic :: Internet :: WWW/HTTP :: WSGI :: Server'
        ],
    ext_modules=ext_modules,
    )
