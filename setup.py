# -*- coding: utf-8 -*-
# Copyright (c) 2010 Jonas Haag <jonas@lophus.org> and contributors.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

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
