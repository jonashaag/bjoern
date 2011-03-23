bjoern: Fast And Ultra-Lightweight Asynchronous HTTP/1.1 WSGI Server
====================================================================

A screamingly fast, ultra-lightweight asynchronous WSGI_ server for CPython,
written in C using Marc Lehmann's high performance libev_ event loop and
Ryan Dahl's http-parser_.

Why It's Cool
~~~~~~~~~~~~~
bjoern is the *fastest*, *smallest* and *most lightweight* WSGI server out there,
featuring

* ~ 1000 lines of C code
* Memory footprint ~ 600KB
* Single-threaded and without coroutines or other crap
* Full persistent connection ("*keep-alive*") support in both HTTP/1.0 and 1.1,
  including support for HTTP/1.1 chunked responses

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
.. _homebrew: http://mxcl.github.com/homebrew/
