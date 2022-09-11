import os
import glob
from setuptools import setup, Extension

long_description = open(os.path.join(os.path.dirname(__file__), "README.rst")).read()

WANT_SIGINT_HANDLING = os.environ.get('BJOERN_WANT_SIGINT_HANDLING', True)
WANT_SIGNAL_HANDLING = os.environ.get('BJOERN_WANT_SIGNAL_HANDLING', True)
SIGNAL_CHECK_INTERVAL = os.environ.get('BJOERN_SIGNAL_CHECK_INTERVAL', '0.1')
WANT_STATSD = os.environ.get('BJOERN_WANT_STATSD', False)
WANT_STATSD_TAGS = os.environ.get('BJOERN_WANT_STATSD_TAGS', False)

compile_flags = [('SIGNAL_CHECK_INTERVAL', SIGNAL_CHECK_INTERVAL)]
if WANT_SIGNAL_HANDLING:
    compile_flags.append(('WANT_SIGNAL_HANDLING', 'yes'))
if WANT_SIGINT_HANDLING:
    compile_flags.append(('WANT_SIGINT_HANDLING', 'yes'))
if WANT_STATSD:
    compile_flags.append(('WANT_STATSD', 'yes'))
    if WANT_STATSD_TAGS:
        compile_flags.append(('WANT_STATSD_TAGS', 'yes'))

SOURCE_FILES = [os.path.join('http-parser', 'http_parser.c')] + \
               [os.path.join('statsd-c-client', 'statsd-client.c')] + \
               sorted(glob.glob(os.path.join('bjoern', '*.c')))

if not WANT_STATSD:
    SOURCE_FILES.remove('statsd-c-client/statsd-client.c')
    SOURCE_FILES.remove('bjoern/statsd_tags.c')

bjoern_extension = Extension(
    '_bjoern',
    sources       = SOURCE_FILES,
    libraries     = ['ev'],
    include_dirs  = ['http-parser', 'statsd-c-client', '/usr/include/libev',
                     '/opt/local/include', '/opt/homebrew/include', '/usr/local/include'],
    library_dirs  = ['/opt/homebrew/lib/', '/usr/local/lib'],
    define_macros = compile_flags,
    extra_compile_args = ['-std=c99', '-fno-strict-aliasing', '-fcommon',
                          '-fPIC', '-Wall', '-Wextra', '-Wno-unused-parameter',
                          '-Wno-missing-field-initializers', '-g'],
)

setup(
    name         = 'bjoern',
    author       = 'Jonas Haag',
    author_email = 'jonas@lophus.org',
    license      = '2-clause BSD',
    url          = 'https://github.com/jonashaag/bjoern',
    description  = 'A screamingly fast Python 2 + 3 WSGI server written in C.',
    version      = '3.2.2',
    long_description = long_description,
    classifiers  = ['Development Status :: 4 - Beta',
                    'License :: OSI Approved :: BSD License',
                    'Programming Language :: C',
                    'Programming Language :: Python :: 2',
                    'Programming Language :: Python :: 3',
                    'Topic :: Internet :: WWW/HTTP :: WSGI :: Server'],
    py_modules   = ['bjoern'],
    ext_modules  = [bjoern_extension]
)
