#!/bin/bash -eux
#
# Build wheels for all python interpreter installed in /opt/python as in
# manylinux containers.
#
# By default, search for a sdist tarball for version described in setup.py.
# Accepts a specific tarball as first argument.
#

top_srcdir="$(readlink -m "$0/../..")"
distdir="${top_srcdir}/dist"

# Ensure to always clean up, even on error.
teardown() {
	# Get outer user ID and GID from self.
	uid_gid="$(stat -c %u:%g "$0")"
	# Ensure dist/ files are owned by user.
	chown -R "$uid_gid" "$distdir"
}
trap teardown INT EXIT TERM

cd "$top_srcdir"

export PIP_DISABLE_PIP_VERSION_CHECK=off
export PIP_NO_PYTHON_VERSION_WARNING=on

runtimes=(/opt/python/cp*)

version="$("${runtimes[0]}/bin/python" setup.py --version)"
tarname="bjoern-$version"
sdist="${1-$distdir/$tarname.tar.gz}"

if ! [ -f "$sdist" ] ; then
	echo "Missing source tarball $sdist." >&2
	exit 1
fi

yum install -q -y libev-devel
if ! rpm --query --queryformat= libev-devel  ; then
	# Looks like libev-devel is not available, like in manylinux1. Let's
	# install from sources.
	LIBEV_VERSION=4.33
	curl http://dist.schmorp.de/libev/Attic/libev-${LIBEV_VERSION}.tar.gz --silent --output /tmp/libev-${LIBEV_VERSION}.tar.gz
	tar xf /tmp/libev-${LIBEV_VERSION}.tar.gz -C /tmp/
	(cd /tmp/libev-${LIBEV_VERSION}; ./configure)
	make -C /tmp/libev-${LIBEV_VERSION}/
	make -C /tmp/libev-${LIBEV_VERSION}/ install
fi

# Blacklist cp27-cp27m as it triggers: site-packages/_bjoern.so: undefined
# symbol: PyUnicodeUCS2_AsLatin1String
runtime_blacklist=(cp27-cp27m)
for runtimedir in "${runtimes[@]}" ; do
	if [[ " ${runtime_blacklist[*]]} " =~ " ${runtimedir##*/} " ]] ; then
		continue
	fi
	PATH="$runtimedir/bin:$PATH" $top_srcdir/packaging/build_wheel.sh "$sdist"
done
