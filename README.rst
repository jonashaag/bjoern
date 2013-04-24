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
* Can bind to TCP `host:port` addresses and Unix sockets (thanks @k3d3!)
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
Fedora, CentOS
   ``yum install libev-devel``
Mac OS X (using homebrew_)
   ``brew install libev``
Your Contribution Here
   Fork me and send a pull request

bjoern
------

Make sure *libev* is installed and then::

   python setup.py install

On some Linux systems (notably Fedora and CentOS), the libev headers may be installed
outside of the default include path. In order to build bjoern you will need to
export ``CFLAGS`` when running ``setup.py``, for instance::

   CFLAGS=-I/usr/include/libev python setup.py install

Usage
~~~~~
::

   # Bind to TCP host/port pair:
   bjoern.run(wsgi_application, host, port)

   # Bind to Unix socket:
   bjoern.run(wsgi_application, 'unix:/path/to/socket')

   # Bind to abstract Unix socket: (Linux only)
   bjoern.run(wsgi_application, 'unix:@socket_name')

Alternatively, the mainloop can be run separately::

   bjoern.listen(wsgi_application, host, port)
   bjoern.run()

.. _WSGI:         http://www.python.org/dev/peps/pep-0333/
.. _libev:        http://software.schmorp.de/pkg/libev.html
.. _http-parser:  https://github.com/joyent/http-parser
.. _homebrew: http://mxcl.github.com/homebrew/
