import os
import random
try:
    import httplib
except ImportError:
    import http.client as httplib
import socket

HOST = ('127.0.0.1', 9000)

class Fail(Exception):
    pass

def dispatcher(environ, start_response):
    return dispatcher.app(environ, start_response)

def still_alive(sock):
    prev_timeout = sock.gettimeout()
    try:
        sock.settimeout(0.5)
        try:
            c = sock.recv(100)
            assert not c, repr(c)
            return False
        except socket.timeout:
            return True
    finally:
        sock.settimeout(prev_timeout)


class NeverClosedHTTPConnection(httplib.HTTPConnection):
    def close(self):
        backup = self.sock
        self.sock = None
        httplib.HTTPConnection.close(self)
        self.sock = backup

class Testcase(object):
    body_length = 1000

    def __init__(self):
        self.conn = None
        self.body = os.urandom(self.body_length)
        self.request_count = 5

    def run(self):
        if not self.conn:
            dispatcher.app = self
            self.conn = NeverClosedHTTPConnection(*HOST)

        self.send_request(self._tinker_request())
        response = self.get_response()
        if self.expect_chunked and not response.chunked:
            raise Fail("Response unexpectedly not chunked")
        body = response.read()

        if self.raise_error:
            if response.status != 500 or body:
                raise Fail("Expected 500 Internal Server Error")
        else:
            if response.status != 200:
                raise Fail("Status is %d, expected 200", response.status)

        if still_alive(self.conn.sock):
            if not self.expect_keep_alive:
                raise Fail("Expected connection not to be kept-alive")
            self.request_count -= 1
            if self.request_count:
                if self.request_count == 1:
                    # Send Connection: close on last request
                    self.want_keep_alive = False
                    self.expect_keep_alive = False
                    self.expect_chunked = False
                self.run()
            else:
                if still_alive(self.conn.sock):
                    raise Fail("Connection still alive")
        else:
            if self.expect_keep_alive:
                raise Fail("Expected connection to be kept-alive")

        if not self.raise_error and body != self.body:
            raise Fail("Different bodies:\n%s\n%s" % (body, self.body))

    def _is_chunked(self, response):
        if not 'Transfer-Encoding: chunked' in response or not response.endswith('0\r\n\r\n'):
            return False
        # TODO: full-featured chunked validation here
        return True

    def _tinker_request(self):
        if self.http_minor == 0:
            req = b'GET / HTTP/1.0\r\n'
            if self.want_keep_alive:
                req += b'Connection: Keep-Alive\r\n'
        else:
            req = b'GET / HTTP/1.1\r\n'
            if not self.want_keep_alive:
                req += b'Connection: close\r\n'
        req += b'\r\n'
        return req

    def send_request(self, data):
        self.conn.send(data)

    def get_response(self):
        self.conn._HTTPConnection__state = httplib._CS_REQ_SENT
        return self.conn.getresponse()

    def __call__(self, environ, start_response):
        if self.raise_error and random.randint(0, 1):
            raise ValueError('foo')
        headers = []
        if self.give_content_length:
            headers.append(('Content-Length', str(len(self.body))))
        if self.raise_error:
            start_response('200 ok', headers)
            raise ValueError('bar')
        start_response('200 ok', headers)
        # second item is to trick bjoern's internal optimizations:
        return [self.body, b'']

try:
    import thread
except ImportError:
    import _thread as thread
import bjoern
thread.start_new_thread(bjoern.run, (dispatcher,)+HOST)

import time; time.sleep(0.1)

for index, tpl in enumerate([
    # HTTP 1.0:
    # - Column "keep-alive" means "keep-alive header sent".
    # - Never do keep-alive unless requested.
    # - Never do chunked (not supported).
    # - Can only keep-alive if content length is given.
    # => Only "keep-alive" case is:
    #    "no-error AND content-length is given AND keep-alive requested"

    # |----------------------- inputs -----------------| |------- outputs -------|
    # | HTTP minor error  content-length   keep-alive  | | chunked    keep alive |
    (       0,      0,          0,              0,          0,          0),
    (       0,      0,          0,              1,          0,          0),
    (       0,      0,          1,              0,          0,          0),
    (       0,      0,          1,              1,          0,          1),
    (       0,      1,          0,              0,          0,          0),
    (       0,      1,          0,              1,          0,          0),
    (       0,      1,          1,              0,          0,          0),
    (       0,      1,          1,              1,          0,          0),

    # HTTP 1.1:
    # - Columns "keep-alive" means "no close header sent"
    # - Do keep-alive by default
    # - Do chunked if content length is missing and keep-alive
    # => Only "close" in case of error.

    # |----------------------- inputs -----------------| |------- outputs -------|
    # | HTTP minor error  content-length   keep-alive  | | chunked    keep alive |
    (       1,      0,          0,              0,          0,          0),
    (       1,      0,          0,              1,          1,          1),
    (       1,      0,          1,              0,          0,          0),
    (       1,      0,          1,              1,          0,          1),
    (       1,      1,          0,              0,          0,          0),
    (       1,      1,          0,              1,          0,          0),
    (       1,      1,          1,              0,          0,          0),
    (       1,      1,          1,              1,          0,          0)
]):
    print('Running test %d: %r' % (index, tpl))
    class _test(Testcase):
        http_minor, \
        raise_error, \
        give_content_length, \
        want_keep_alive, \
        expect_chunked, \
        expect_keep_alive = tpl
    _test().run()

print('--- SUCCESS! ---')
