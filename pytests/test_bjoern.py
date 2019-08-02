# -*- coding: utf-8 -*-

import contextlib
import json
import math
import os
import unittest
import signal
import socket
import sys
import textwrap
from wsgiref.validate import validator

import pytest
import requests
import bjoern


@pytest.mark.parametrize('header_x_auth', ['X-Auth_User', 'X_Auth_User', 'X_Auth-User'])
def test_CVE_2015_0219(client, header_x_auth):
    """
    https://www.djangoproject.com/weblog/2015/jan/13/security/
    https://snyk.io/vuln/SNYK-PYTHON-BJOERN-40507
    """

    def app(env, start_response):
        assert header_x_auth not in env.keys()
        assert 'HTTP_X_AUTH_USER' not in env.keys()
        start_response('200 ok', [])
        return b''

    client.start(app)
    client.get(headers={header_x_auth: 'admin'})


def test_listen(client):
    """tests/listen.py"""

    def app(environ, start_response):
        start_response('200 OK', [])
        yield b'Hello world'
        yield b''

    client.start(app)
    assert client.get().content == b'Hello world'


@pytest.mark.parametrize(
    'content,status_code', [('200 ok', 200), ('201 created', 201), ('202 accepted', 202), ('204 no content', 204)]
)
def test_status_codes(client, content, status_code):
    """tests/204.py"""

    def app(e, s):
        s(content, [])
        return b''

    client.start(app)
    resp = client.get()
    assert resp.status_code == status_code


def test_empty_content(client):
    """tests/empty.py"""

    def app(e, s):
        s('200 ok', [])
        return b''

    client.start(app)
    response = client.get()
    assert response.content == b''


@pytest.mark.parametrize('method', ['DELETE', 'GET', 'OPTIONS', 'POST', 'PATCH', 'PUT'])
def test_env_request_method(client, method):
    """tests/env.py"""

    def app(env, start_response):
        start_response('200 yo', [])
        assert env['REQUEST_METHOD'] == method
        return []

    client.start(app)
    m = getattr(client, method.lower())
    m()


def wrap(text, width=20, placeholder='...'):
    wrapped = textwrap.wrap(text, width=width)[0]
    if len(wrapped) < len(text):
        wrapped += placeholder
    return wrapped


@pytest.mark.parametrize(
    'header_key,header_value',
    [
        ('Content-Type', 'text/plain'),
        ('a' * 1000, 'b' * 1000),
        ('Foo', 'Bar'),
        ('Blah', 'Blubb'),
        ('Spam', 'Eggs'),
        ('Blurg', 'asdasjdaskdasdjj asdk jaks / /a jaksdjkas jkasd jkasdj '),
        ('asd2easdasdjaksdjdkskjkasdjka', 'oasdjkadk kasdk k k k k k '),
    ],
    ids=wrap,
)
def test_headers(client, header_key, header_value):
    """tests/headers.py"""

    def app(env, start_response):
        start_response('200 yo', [(header_key, header_value)])
        return [b'foo', b'bar']

    client.start(app)
    resp = client.get()
    assert header_key in resp.headers.keys()
    assert resp.headers[header_key] == header_value


def test_invalid_header_type(client):
    """tests/all-kinds-of-errors.py"""

    def app(environ, start_response):
        start_response('200 ok', None)
        return ['yo']

    client.start(app)
    with pytest.raises(TypeError):
        client.get()


@pytest.mark.parametrize('headers', [(), ('a', 'b', 'c'), ('a',)], ids=str)
def test_invalid_header_tuple(client, headers):
    """tests/all-kinds-of-errors.py"""

    def app(environ, start_response):
        start_response('200 ok', headers)
        return ['yo']

    client.start(app)
    message = r"start response argument 2 must be a list of 2-tuples \(got 'tuple' object instead\)"
    with pytest.raises(TypeError, match=message):
        client.get()


def test_invalid_header_tuple_item(client):
    """tests/all-kinds-of-errors.py"""

    def app(environ, start_response):
        start_response('200 ok', (object(), object()))
        return ['yo']

    client.start(app)
    message = r"start response argument 2 must be a list of 2-tuples \(got 'tuple' object instead\)"
    with pytest.raises(TypeError, match=message):
        client.get()


@pytest.fixture(params=[('small.tmp', 888), ('big.tmp', 88888)], ids=lambda s: s[0])
def temp_file(request, tmp_path):
    name, size = request.param
    file = tmp_path / name
    with file.open('wb') as f:
        f.write(os.urandom(size))
    return file


def test_send_file(client, temp_file):
    """tests/file.py"""

    def app(env, start_response):
        start_response('200 ok', [])
        return temp_file.open('rb')

    client.start(app)
    response = client.get()
    assert response.content == temp_file.read_bytes()


class PseudoFile:
    def __iter__(self):
        return self

    def __next__(self):
        return b'ab'

    def next(self):
        return self.__next__()

    def read(self, *ignored):
        return b'ab'


def filewrapper_factory(wrapper, file, pseudo=False):
    def app(env, start_response):
        f = PseudoFile() if pseudo else open(file, 'rb')
        wrapped = wrapper(f, env)
        start_response('200 ok', [('Content-Length', str(os.path.getsize(file)))])
        return wrapped

    return app


@pytest.mark.parametrize('file', ['README.rst'], ids=['README.rst'])
@pytest.mark.parametrize('pseudo', [False, True], ids=lambda x: 'pseudofile' if x else 'file')
@pytest.mark.parametrize(
    'wrapper',
    [
        lambda f, _: iter(lambda: f.read(64 * 1024), b''),
        lambda f, _: f,
        lambda f, env: env['wsgi.file_wrapper'](f),
        lambda f, env: env['wsgi.file_wrapper'](f, 1),
    ],
    ids=['callable-iterator', 'xreadlines', 'filewrapper', 'filewrapper2'],
)
def test_file_wrapper(client, wrapper, file, pseudo):
    """tests/filewrapper.py"""
    client.start(filewrapper_factory(wrapper, file, pseudo))
    resp = client.get()
    resp.raise_for_status()

    if pseudo:
        size = os.path.getsize(file)
        text = PseudoFile().read()
        expected = (text * int(2 * size / len(text)))[:size]
    else:
        with open(file, 'rb') as fp:
            expected = fp.read()
    assert resp.content == expected


def test_wsgi_app_not_callable(client):
    """tests/not-callable.py"""
    client.start(object())
    with pytest.raises(TypeError):
        response = client.get()


def test_iter_response(client):
    """tests/huge.py"""
    N = 1024
    CHUNK = b'a' * 1024
    DATA_LEN = N * len(CHUNK)

    class _iter(object):
        def __iter__(self):
            for i in range(N):
                yield CHUNK

    def app(e, s):
        s('200 ok', [('Content-Length', str(DATA_LEN))])
        return _iter()

    client.start(app)
    response = client.get()
    assert response.content == b'a' * 1024 * 1024


def test_tuple_response(client):
    """tests/slow_server.py"""

    def app(environ, start_response):
        start_response('200 OK', [('Content-Type', 'text/plain')])
        return (b'Hello,', b" it's me, ", b'Bob!')

    client.start(app)
    response = client.get()
    assert response.content == b"Hello, it's me, Bob!"


def test_huge_response(client):
    """tests/slow_server.py"""

    def app(environ, start_response):
        start_response('200 OK', [('Content-Type', 'text/plain')])
        return [b'x' * (1024 * 1024)]

    client.start(app)
    response = client.get()
    assert response.content == b'x' * 1024 * 1024


def app1(env, sr):
    headers = [
        ('Foo', 'Bar'),
        ('Blah', 'Blubb'),
        ('Spam', 'Eggs'),
        ('Blurg', 'asdasjdaskdasdjj asdk jaks / /a jaksdjkas jkasd jkasdj '),
        ('asd2easdasdjaksdjdkskjkasdjka', 'oasdjkadk kasdk k k k k k '),
    ]
    sr('200 ok', headers)
    return [b'hello', b'world']


def app2(env, sr):
    sr('200 ok', [])
    return b'hello'


def app3(env, sr):
    sr('200 abc', [('Content-Length', '12')])
    yield b'Hello'
    yield b' World'
    yield b'\n'


def app4(e, sr):
    sr('200 ok', [])
    return [b'hello there ... \n']


@pytest.mark.parametrize(
    'app,status_code,body',
    [
        (app1, 200, b'helloworld'),
        (app2, 200, b'hello'),
        (app3, 200, b'Hello World\n'),
        (app4, 200, b'hello there ... \n'),
    ],
)
def test_unix_socket(unixclient, app, status_code, body):
    """tests/hello_unix.py"""
    unixclient.start(app)
    response = unixclient.get()
    assert response.status_code == status_code
    assert response.content == body


def test_interrupt_during_request(client):
    """tests/interrupt-during-request.py"""

    def application(environ, start_response):
        start_response('200 ok', [])
        yield b'chunk1'
        os.kill(os.getpid(), signal.SIGINT)
        yield b'chunk2'
        yield b'chunk3'

    client.start(application)
    assert client.get().content == b'chunk1chunk2chunk3'


def test_exc_info_reference(client):
    """tests/test_exc_info_reference.py"""

    def app(env, start_response):
        start_response('200 alright', [])
        try:
            a
        except:
            import sys

            x = sys.exc_info()
            start_response('500 error', [], x)
        return [b'hello']

    client.start(app)
    response = client.get()
    assert response.status_code == 500
    assert response.content == b'hello'


def test_fork(unixclient):
    """tests/fork.py"""

    def app(env, start_response):
        start_response('200 ok', [])
        return str(os.getpid()).encode('ascii')

    unixclient.start(app, num_workers=10)
    pids = {unixclient.get().text for _ in range(100)}
    assert len(pids) > 1


def test_wsgi_compliance(client):
    """test_wsgi_compliance.py"""

    @validator
    def _app(environ, start_response):
        start_response('200 OK', [('Content-Type', 'text/plain')])
        return [b'Hello World']

    client.start(_app)
    client.get()


@pytest.mark.parametrize(
    'expect_header_value,content_length,body,response',
    [
        ('100-continue', 300, '', b'HTTP/1.1 100 Continue\r\n\r\n'),
        ('100-continue', 0, '', b'HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: Keep-Alive\r\n\r\n'),
        ('100-continue', 4, 'test', b'HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: Keep-Alive\r\n\r\n'),
        ('badness', 0, '', b'HTTP/1.1 417 Expectation Failed\r\n\r\n'),
        ('badness', 300, '', b'HTTP/1.1 417 Expectation Failed\r\n\r\n'),
        ('badness', 4, 'test', b'HTTP/1.1 417 Expectation Failed\r\n\r\n'),
    ],
)
def test_expect_100_continue(httpclient, expect_header_value, content_length, body, response):
    """tests/expect100.py"""

    def app(e, s):
        s('200 OK', [('Content-Length', '0')])
        return b''

    try:
        socket.SO_REUSEPORT
    except AttributeError:
        httpclient.reuse_port = False
    else:
        httpclient.reuse_port = True

    httpclient.start(app)
    # requests doesn't support expect100, see https://github.com/psf/requests/issues/3614
    # use the raw connection instead of httpclient
    with contextlib.closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as raw_client:
        raw_client.connect((httpclient.host, httpclient.port))
        request = 'GET /fizz HTTP/1.1\r\nExpect: {}\r\nContent-Length: {}\r\n\r\n{}'.format(
            expect_header_value, content_length, body
        )
        raw_client.send(request.encode('utf-8'))
        resp = raw_client.recv(1 << 10)
        assert resp == response

        if resp == b'HTTP/1.1 100 Continue\r\n\r\n':
            body = ''.join('x' for x in range(0, content_length))
            raw_client.send(body.encode('utf-8'))
            resp = raw_client.recv(bjoern.DEFAULT_LISTEN_BACKLOG)
            assert resp == b'HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: Keep-Alive\r\n\r\n'
