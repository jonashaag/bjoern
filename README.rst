bjoern
======

A screamingly fast CPython `WSGI`_ server for CPython, written in C
using Marc Lehmann's high performance `libev`_ event loop
and Ryan Dahl's `http_parser`_.

bjoern aims to be *small*, *lightweight* and *very fast*.

* less than 800 SLOC (Source Lines Of Code)
* memory footprint smaller than a megabyte
* no threads, coroutines or other crap
* apparently the fastest WSGI server out there
* 100% WSGI compliant (except for the `write callback design mistake`_)

.. _WSGI:         http://www.python.org/dev/peps/pep-0333/
.. _libev:        http://software.schmorp.de/pkg/libev.html
.. _http_parser:  http://github.com/ry/http-parser
.. _write callback design mistake:
                  http://www.python.org/dev/peps/pep-0333/#the-write-callable

But what about...
~~~~~~~~~~~~~~~~~
Fapws3?
-------
Sucks. Really, the code is an incredible mess. It likes to segfault.

I tried to patch Fapws so that it would support threading,
but after about two hours of brain slime feeling, I decided
to write my own WSGI server -- "Fawps done right".

gunicorn?
---------
bjoern is about 5 times faster. Plus gunicorn is too much bloated.

uWSGI?
------
Awesome project, but way too much bloat.

meinheld?
---------
Unfortunately now bloated with gunicorn and coroutine/greenlet crap,
seemed to be a very nice server at first.

$SERVER?
--------
Probably too much bloat, too slow, does not scale, buggy, ...
