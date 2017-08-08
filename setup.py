import os
import glob
from setuptools import setup, Extension

SOURCE_FILES = [os.path.join('http-parser', 'http_parser.c')] + \
               sorted(glob.glob(os.path.join('bjoern', '*.c')))

bjoern_extension = Extension(
    '_bjoern',
    sources       = SOURCE_FILES,
    libraries     = ['ev'],
    include_dirs  = ['http-parser', '/usr/include/libev'],
    define_macros = [('WANT_SENDFILE', '1'),
                     ('WANT_SIGINT_HANDLING', '1')],
    extra_compile_args = ['-fno-strict-aliasing', '-fcommon',
                          '-fPIC', '-Wall', '-Wextra', '-Wno-unused-parameter',
                          '-Wno-missing-field-initializers', '-g']
)

setup(
    name         = 'bjoern',
    author       = 'Jonas Haag',
    author_email = 'jonas@lophus.org',
    license      = '2-clause BSD',
    url          = 'https://github.com/jonashaag/bjoern',
    description  = 'A screamingly fast Python WSGI server written in C.',
    version      = '1.4.3',
    classifiers  = ['Development Status :: 4 - Beta',
                    'License :: OSI Approved :: BSD License',
                    'Programming Language :: C',
                    'Programming Language :: Python :: 2',
                    'Topic :: Internet :: WWW/HTTP :: WSGI :: Server'],
    py_modules   = ['bjoern'],
    ext_modules  = [bjoern_extension]
)
