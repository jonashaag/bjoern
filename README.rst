bjoern: Fast And Ultra-Lightweight Asynchronous WSGI Server
===========================================================

A screamingly fast, ultra-lightweight asynchronous WSGI_ server for CPython,
written in C using Marc Lehmann's high performance libev_ event loop and
Ryan Dahl's http_parser_.

Why It's Cool
~~~~~~~~~~~~~
bjoern is the *fastest*, *smallest* and *most lightweight* WSGI server out there,
featuring

* < 1000 lines of code
* Memory footprint ~ 600KB
* Single-threaded and without coroutines or other crap
* 100% WSGI compliance (except for the `write callback design mistake`_)

What's Not So Cool
------------------
* Not HTTP/1.1 capable (yet).

Installation
~~~~~~~~~~~~
libev
-----
Arch Linux
   ``pacman -S libev``
Ubuntu
   ``apt-get install libev-dev``
Mac OS X (using homebrew_)
   ``brew install libev``
Your Contribution Here
   Fork me and send a pull request

bjoern
------
Make sure *libev* is installed and then::

   pip install bjoern

Usage
~~~~~
::

   bjoern.run(wsgi_application, host, port)

Alternatively, the mainloop can be run separately::

   bjoern.listen(wsgi_application, host, port)
   bjoern.run()

.. _WSGI:         http://www.python.org/dev/peps/pep-0333/
.. _libev:        http://software.schmorp.de/pkg/libev.html
.. _http_parser:  http://github.com/ry/http-parser
.. _write callback design mistake:
                  http://www.python.org/dev/peps/pep-0333/#the-write-callable
.. _homebrew: http://mxcl.github.com/homebrew/
