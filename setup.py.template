import os
from distutils.core import setup, Extension

# make print-env
stdout = '''
CFLAGS=-D WANT_SENDFILE -D WANT_SIGINT_HANDLING -std=c99 -fno-strict-aliasing -Wall -Wextra -Wno-unused -g -O3 -fPIC
CPPFLAGS=-I /usr/include/python2.7/ -I . -I bjoern -I http-parser
LDFLAGS=-l python2.7 -l ev -shared --as-needed
args=http_parser.c request.c bjoernmodule.c server.c wsgi.c'''

env = dict(line.split('=', 1) for line in stdout.split('\n') if '=' in line)

source_files = env.pop('args').split()

os.environ.update(env)

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
    ext_modules  = [Extension('bjoern', sources=source_files, libraries=['ev'])]
)
