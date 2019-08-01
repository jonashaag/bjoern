from tblib import pickling_support

pickling_support.install()

import contextlib
import functools
import multiprocessing as mp
import os
import posixpath
import signal
import socket
import sys
import tempfile
from six import reraise as raise_
from six.moves.urllib.parse import quote, urljoin, urlunsplit

import pytest
import requests
import requests_unixsocket

import bjoern


def free_port():
    with contextlib.closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        s.bind(('', 0))
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        port = s.getsockname()[1]
    return port


class ErrorHandleMiddleware:
    def __init__(self, app, pipe=None):
        self.app = app
        self.pipe = pipe

    def __call__(self, *args, **kwargs):
        try:
            return self.app(*args, **kwargs)
        except Exception as e:
            if self.pipe is not None:
                exc_info = sys.exc_info()
                exc_type = exc_info[0]
                tb = exc_info[2]
                self.pipe.send((exc_type, e, tb))
            raise e


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_call(item):
    yield
    if client.__name__ in item.funcargs.keys():
        # check for uncaught errors in server process, raise if any
        testclient = item.funcargs[client.__name__]
        testclient._reraise_app_errors()


def reraise(f):
    @functools.wraps(f)
    def wrapper(*args, **kwargs):
        result = f(*args, **kwargs)
        testclient = args[0]
        testclient._reraise_app_errors()
        return result

    return wrapper


class Client(object):
    host = None
    port = None

    _error_publisher = None
    _error_receiver = None
    _proc = None
    _session = None
    _sock = None

    def __init__(self):
        self.reuse_port = False
        self.listen_backlog = bjoern.DEFAULT_LISTEN_BACKLOG
        self._children = []

    @property
    def session(self):
        return self._session

    def __enter__(self):
        return self

    def __exit__(self, type, value, tb):
        self.stop()

    def start(self, wsgi_app, num_workers=1):
        assert self._sock is not None
        num_workers = max(num_workers, 1)
        (self._error_receiver, self._error_publisher) = mp.Pipe(False)
        for _ in range(num_workers):
            errorhandler = ErrorHandleMiddleware(wsgi_app, self._error_publisher)
            child = mp.Process(
                target=bjoern.server_run, args=(self._sock, errorhandler)
            )
            child.start()
            self._children.append(child)

    def stop(self):
        if self._sock is None:
            return
        self.session.close()
        self._error_publisher.close()
        self._error_receiver.close()
        for child in self._children:
            os.kill(child.pid, signal.SIGTERM)
            child.join()
        if self._sock.family == socket.AF_UNIX:
            filename = self._sock.getsockname()
            if filename[0] != '\0':
                os.unlink(self._sock.getsockname())
        self._sock.close()

    def _reraise_app_errors(self, timeout=0):
        exc_info = None
        try:
            if self._error_receiver.poll(timeout):
                exc_info = self._error_receiver.recv()
        except EOFError:
            return
        if exc_info is not None:
            raise_(*exc_info)  # python2 compat

    @reraise
    def get(self, path=None, **kwargs):
        """Adapter for :py:meth:`requests.Session.get`"""
        return self.session.get(self._join(self.root, (path or '')), **kwargs)

    @reraise
    def options(self, path=None, **kwargs):
        """Adapter for :py:meth:`requests.Session.options`"""
        return self.session.options(self._join(self.root, (path or '')), **kwargs)

    @reraise
    def head(self, path=None, **kwargs):
        """Adapter for :py:meth:`requests.Session.head`"""
        return self.session.head(self._join(self.root, (path or '')), **kwargs)

    @reraise
    def post(self, path=None, data=None, json=None, **kwargs):
        """Adapter for :py:meth:`requests.Session.post`"""
        return self.session.post(
            self._join(self.root, (path or '')), data=data, json=json, **kwargs
        )

    @reraise
    def put(self, path=None, data=None, **kwargs):
        """Adapter for :py:meth:`requests.Session.put`"""
        return self.session.put(
            self._join(self.root, (path or '')), data=data, **kwargs
        )

    @reraise
    def patch(self, path=None, data=None, **kwargs):
        """Adapter for :py:meth:`requests.Session.patch`"""
        return self.session.patch(
            self._join(self.root, (path or '')), data=data, **kwargs
        )

    @reraise
    def delete(self, path=None, **kwargs):
        """Adapter for :py:meth:`requests.Session.delete`"""
        return self.session.delete(self._join(self.root, (path or '')), **kwargs)


class HttpClient(Client):
    _join = functools.partial(urljoin)

    def __init__(self, host='127.0.0.1', port=None):
        self.host = host
        self.port = port
        super(HttpClient, self).__init__()

    def start(self, wsgi_app, num_workers=1):
        self._session = requests.Session()
        self.port = self.port or free_port()
        self._sock = bjoern.bind_and_listen(
            self.host,
            port=self.port,
            reuse_port=self.reuse_port,
            listen_backlog=self.listen_backlog,
        )
        super(HttpClient, self).start(wsgi_app, num_workers)

    @property
    def root(self):
        return urlunsplit(('http', '{}:{}'.format(self.host, self.port), '', '', ''))


class UnixClient(Client):
    _join = functools.partial(posixpath.join)

    def start(self, wsgi_app, num_workers=1):
        self._session = requests_unixsocket.Session()
        self._sock = bjoern.bind_and_listen(
            'unix:{}'.format(self.host),
            port=self.port,
            listen_backlog=self.listen_backlog,
        )
        super(UnixClient, self).start(wsgi_app, num_workers)

    @property
    def root(self):
        return urlunsplit(('http+unix', quote(self.host, safe=''), '', '', ''))


@pytest.fixture(params=('HttpClient', 'UnixClient'))
def client(request):
    return request.getfixturevalue(request.param.lower())


@pytest.fixture
def httpclient():
    with HttpClient() as client:
        yield client


@pytest.fixture
def unixclient():
    """
    * don't use pytest's tmpdir/tmp_path as the generated paths easily exceed 108 chars,
    causing an "OSError: AF_UNIX path too long" on MacOS
    * also, filenames longer than 64 chars cause urllib hickups, see https://bugs.python.org/issue32958
    * another thing: tempfile.gettempdir() on MacOS resorts to $TMPDIR
    which is autogenerated into /var/folders and is also longer than 64 chars
    """
    if sys.platform == 'darwin':
        tmpdir = '/tmp'  # yikes! but should be short enough
    else:
        tmpdir = tempfile.gettempdir()
    with tempfile.NamedTemporaryFile(
        prefix='bjoern', suffix='.sock', dir=tmpdir, delete=True
    ) as temp:
        file = temp.name
    with UnixClient() as client:
        client.host = file
        yield client
