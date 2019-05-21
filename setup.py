import os
import glob
import platform

import sys
from setuptools import setup, Extension
import bjoern

VERSION = bjoern.__version__

SOURCE_FILES = [
    os.path.join("vendors", "http-parser", "http_parser.c"),
    os.path.join("vendors", "http-parser", "contrib", "url_parser.c"),
] + sorted(glob.glob(os.path.join("src", "*.c")))

# if (
#     "Linux" != platform.system()
#     or "CPython" != platform.python_implementation()
#     or not sys.version_info >= (3, 6)
# ):
#     print("Sorry, not support for other platforms than CPython3.6+ Linux.")
#     sys.exit(1)


_DEBUG = bool(os.environ.get("DEBUG", False))

if _DEBUG:
    extra_compile_args = [
        "-std=c99",
        "-pthread",
        "-fno-strict-aliasing",
        "-fcommon",
        "-fPIC",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-missing-field-initializers",
        "-D DEBUG",
    ]
else:
    extra_compile_args = [
        "-std=c99",
        "-pthread",
        "-fno-strict-aliasing",
        "-fcommon",
        "-fPIC",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-missing-field-initializers",
        "-Os",
        "-s",
        "-O3",
        "-march=core-avx2",
        "-mtune=core-avx2",
    ]
bjoern_extension = Extension(
    "_bjoern",
    sources=SOURCE_FILES,
    libraries=["ev"],
    include_dirs=["vendors/http-parser", "/usr/include/libev"],
    define_macros=[
        ("WANT_SENDFILE", "1"),
        ("WANT_SIGINT_HANDLING", "1"),
        ("WANT_SIGNAL_HANDLING", "1"),
        ("SIGNAL_CHECK_INTERVAL", "0.1"),
    ],
    extra_compile_args=extra_compile_args,
)

setup(
    name="bjoern",
    author="Jonas Haag",
    author_email="jonas@lophus.org",
    license="2-clause BSD",
    url="https://github.com/jonashaag/bjoern",
    description="A screamingly fast Python 2 + 3 WSGI server written in C.",
    version=VERSION,
    include_package_data=True,
    classifiers=[
        "Development Status :: 5 - Production",
        "License :: OSI Approved :: BSD License",
        "Programming Language :: C",
        "Programming Language :: Python :: 3",
        "Topic :: Internet :: WWW/HTTP :: WSGI :: Server",
    ],
    py_modules=[
        "bjoern",
        "bjoern.server",
        "bjoern.bench",
        "bjoern.bench.flask_bench",
        "bjoern.bench.bottle_bench",
        "bjoern.bench.falcon_bench",
        "bjoern.gworker",
    ],
    ext_modules=[bjoern_extension],
)
