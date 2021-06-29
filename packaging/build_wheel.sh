#!/bin/bash -eux
#
# This script accepts a source tarball as argument and product final wheel for
# current python interpreter. The script saves the wheel in source tarball
# directory and pen test it to validate build.
#

sdist="$1"
basename=${sdist/%.tar.gz}
wheeldir=${sdist%/*}

srcdir=$(readlink -m "$0/..")
wheeltag="$(python "$srcdir/wheeltag.py")"
impl="${wheeltag% *}"
default_platform="${wheeltag#* }"

# Build wheel from source tarball
pip --verbose wheel --no-deps --wheel-dir "$wheeldir" "$sdist"
wheel="$basename-$impl-$default_platform.whl"

# Fix wheel to embed libev shared object.
auditwheel repair --wheel-dir "$wheeldir" "$wheel"
rm -f "$wheel"
wheel="$basename-$impl-$AUDITWHEEL_PLAT.whl"

# Build a venv for pentesting
venv=/tmp/bjoern-pentestenv
rm -rf "$venv"
if python -m venv --help &>/dev/null ; then
  python -m venv "$venv"
else
  # For Python 2.7
  pip install virtualenv
  hash -r
  virtualenv "$venv"
fi
"$venv/bin/pip" install --upgrade pip

# Pentest wheel.
$venv/bin/pip install "$wheel"
cd /  # Move outside source tree.
$venv/bin/python "$srcdir/../tests/test_wsgi_compliance.py"
rm -rf $venv
