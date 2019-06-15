# bjoern: Fast And Ultra-Lightweight HTTP/1.1 WSGI Server (http://www.python.org/dev/peps/pep-0333/)

[![Join the chat at https://gitter.im/jonashaag/bjoern](https://gitter.im/jonashaag/bjoern?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Build Status](https://travis-ci.com/danigosa/bjoern.svg?branch=4.0)](https://travis-ci.com/danigosa/bjoern)

A screamingly fast, ultra-lightweight WSGI_ server for CPython,
written in C using Marc Lehmann's high performance [libev](http://software.schmorp.de/pkg/libev.html) event loop and
Ryan Dahl's [http-parser](https://github.com/nodejs/http-parser).

## Python versions

- Current version 4.x+ only supports CPython3.6+.
- For those looking for CPython2.7, CPython3.4 or CPython3.5, please use Bjoern 3.x.

# Why It's Cool

bjoern is the *fastest*, *smallest* and *most lightweight* WSGI server out there,
featuring

* ~ 1000 lines of C code
* Memory footprint ~ 600KB
* Python 2 and Python 3 support (thanks @yanghao!)
* Single-threaded and without coroutines or other crap
* Can bind to TCP `host:port` addresses and Unix sockets (thanks @k3d3!)
* Full persistent connection ("*keep-alive*") support in both HTTP/1.0 and 1.1,
  including support for HTTP/1.1 chunked responses
* Gunicorn Worker support (4.0+ only)
* Easy development with docker
* Full suite of tests and benchmarks


# Differences between 4.x+ and 3.x:

- 4.x is Python 3.6+ only, tested on Ubuntu Bionic (CPython 3.6.8 and CPython 3.7.2)
- Gunicorn support is only for 4.x and on
- 4.x uses modern `http-parser` version (2.9.2+), and its url parser. 3.x uses an early version and a custom url parser.
- 4.x has _gunicorn-like_ anti DoS features (body length, url legth, headers number and header field length limits)
- 4.x has docker utilities for development and testing
- 3.x is slightly faster, a 5-20% depending on the WSGI framework


# Requirements

Arch Linux

```
pacman -S libev
```

Ubuntu/Debian

```
apt-get install libev-dev
```

Fedora, CentOS

```
yum install libev-devel
```

# Installation

```bash
pip install bjoern
```

For CPython2.7 or CPython<3.6

```bash
pip install bjoern<4.0
```

# Usage

## Standalone

```python
   # Bind to TCP host/port pair:
   bjoern.run(wsgi_application, host, port)

   # TCP host/port pair, enabling SO_REUSEPORT if available.
   bjoern.run(wsgi_application, host, port, reuse_port=True)

   # Bind to Unix socket:
   bjoern.run(wsgi_application, 'unix:/path/to/socket')

   # Bind to abstract Unix socket: (Linux only)
   bjoern.run(wsgi_application, 'unix:@socket_name')
```

Alternatively, the mainloop can be run separately:

```python
bjoern.listen(wsgi_application, host, port)
bjoern.run()
```

You can also simply pass a Python socket(-like) object. Note that you are responsible
for initializing and cleaning up the socket in that case.

```python
   bjoern.server_run(socket_object, wsgi_application)
   bjoern.server_run(filedescriptor_as_integer, wsgi_application)
```

## Gunicorn worker

Use `--worker-class bjoern.gworker.BjoernWorker`:

```bash
pip3 install bjoern>=4.0
gunicorn your:app \
    --bind localhost:8080 \
    --log-level info \
    --workers 4 \
    --backlog 2048 \
    --timeout 1800 \
    --keep-alive 3600 \
    --worker-class bjoern.gworker.BjoernWorker
```

# Development

Requirements:

- docker

Run:

```bash
run-dev
```

Pass tests:

```bash
# make all-36
# make all-37
```

# Benchmarks

```bash
run-dev
# make bjoern-bench-36
# make bjoern-bench-37
```


