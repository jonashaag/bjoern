bjoern: Fast And Ultra-Lightweight Asynchronous WSGI Server
===========================================================

A screamingly fast, ultra-lightweight asynchronous WSGI_ server for CPython,
written in C using Marc Lehmann's high performance libev_ event loop and
Ryan Dahl's `http_parser`_.

Why it is Cool
--------------
``bjoern`` is the *fastest*, *smallest* and *most lightweight* WSGI server out
there featuring:

* < 1000 lines of code
* Memory footprint ~ 600KB
* Single-threaded and does not use coroutines or other crap.
* 100% WSGI compliance (except for the `write callback design mistake`_)


Installing Dependencies
-----------------------
Arch Linux::

    pacman -S libev

Ubuntu Linux::

    apt-get install libev

Mac OS X (using homebrew_)::

    brew install libev

Other platforms may require installing libev_ from source.


Installation
------------
Make sure all the dependencies mentioned in the previous section are installed
before issuing the following command::

   pip install bjoern


Usage
-----
Using ``bjoern`` is easy. You can run a WSGI application directly using
the ``bjoern.run`` function::

   bjoern.run(wsgi_application, host, port)

Alternatively, you can run the main event loop separately::

   bjoern.listen(wsgi_application, host, port)
   bjoern.run()


Compared with other WSGI servers
--------------------------------
There are existing servers that attempt to deal with WSGI but perform poorly for
the reasons highlighted in this comparison table:

+-----------------+---------+--------+------------+
| Name            | Bloated | Slower | Messy Code |
+=================+=========+========+============+
| Fapws3_         |         |        |    Yes     |
+-----------------+---------+--------+------------+
| gunicorn_       |   Yes   |  Yes   |            |
+-----------------+---------+--------+------------+
| uWSGI_          |   Yes   |        |            |
+-----------------+---------+--------+------------+
| meinheld_       |   Yes   |        |            |
+-----------------+---------+--------+------------+
| $SERVER         |   Yes   |  Yes   |    Yes     |
+-----------------+---------+--------+------------+


.. links:

.. _WSGI:         http://www.python.org/dev/peps/pep-0333/
.. _libev:        http://software.schmorp.de/pkg/libev.html
.. _http_parser:  http://github.com/ry/http-parser
.. _write callback design mistake:
                  http://www.python.org/dev/peps/pep-0333/#the-write-callable
.. _homebrew:     http://mxcl.github.com/homebrew/
.. _meinheld:     https://github.com/mopemope/meinheld
.. _uWSGI:        http://projects.unbit.it/uwsgi/
.. _gunicorn:     http://gunicorn.org/
.. _Fapws3:       http://www.fapws.org/
