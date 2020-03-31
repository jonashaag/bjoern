# Exports interpreter PEP425 tags for shell processing.
import sys
from distutils.util import get_platform

from wheel.pep425tags import (
    get_impl_ver,
    get_abi_tag,
    get_abbr_impl,
)

sys.stdout.write("%s%s-%s %s\n" % (
    get_abbr_impl(),
    get_impl_ver(),
    get_abi_tag(),
    get_platform().replace('-', '_'),
))
