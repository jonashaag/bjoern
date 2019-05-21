import os
import signal
import socket
import time

import pytest
from bjoern.tests.conftest import _run_app

try:
    import httplib
except ImportError:
    import http.client as httplib


class Fail(Exception):
    pass


class NeverClosedHTTPConnection(httplib.HTTPConnection):
    def close(self):
        backup = self.sock
        self.sock = None
        httplib.HTTPConnection.close(self)
        self.sock = backup


class App:
    def __init__(
        self,
        http_minor,
        raise_error,
        give_content_length,
        want_keep_alive,
        expect_chunked,
        expect_keep_alive,
        body,
        body_length,
    ):

        self.body = body
        self.body_length = body_length
        self.conn = NeverClosedHTTPConnection(*("127.0.0.1", 8080))
        self.request_count = 5
        self.http_minor = http_minor
        self.raise_error = raise_error
        self.give_content_length = give_content_length
        self.want_keep_alive = want_keep_alive
        self.expect_chunked = expect_chunked
        self.expect_keep_alive = expect_keep_alive

    def __call__(self, environ, start_response):
        headers = []
        if self.give_content_length:
            headers.append(("Content-Length", str(len(self.body))))
        if self.raise_error:
            start_response("200 ok", headers)
            raise ValueError("Yaw")
        start_response("200 ok", headers)
        # second item is to trick bjoern's internal optimizations:
        return [self.body, b""]


LENGTH = 1024
BODY = os.urandom(LENGTH)
apps = [
    App(*tpl + (BODY, LENGTH))
    for _, tpl in enumerate(
        [
            # HTTP 1.0:
            # - Column "keep-alive" means "keep-alive header sent".
            # - Never do keep-alive unless requested.
            # - Never do chunked (not supported).
            # - Can only keep-alive if content length is given.
            # => Only "keep-alive" case is:
            #    "no-error AND content-length is given AND keep-alive requested"
            # |----------------------- inputs -----------------| |------- outputs -------|
            # | HTTP minor error  content-length   keep-alive  | | chunked    keep alive |
            (0, 0, 0, 0, 0, 0),
            (0, 0, 0, 1, 0, 0),
            (0, 0, 1, 0, 0, 0),
            (0, 0, 1, 1, 0, 1),
            (0, 1, 0, 0, 0, 0),
            (0, 1, 0, 1, 0, 0),
            (0, 1, 1, 0, 0, 0),
            (0, 1, 1, 1, 0, 0),
            # HTTP 1.1:
            # - Columns "keep-alive" means "no close header sent"
            # - Do keep-alive by default
            # - Do chunked if content length is missing and keep-alive
            # => Only "close" in case of error.
            # |----------------------- inputs -----------------| |------- outputs -------|
            # | HTTP minor error  content-length   keep-alive  | | chunked    keep alive |
            (1, 0, 0, 0, 0, 0),
            (1, 0, 0, 1, 1, 1),
            (1, 0, 1, 0, 0, 0),
            (1, 0, 1, 1, 0, 1),
            (1, 1, 0, 0, 0, 0),
            (1, 1, 0, 1, 0, 0),
            (1, 1, 1, 0, 0, 0),
            (1, 1, 1, 1, 0, 0),
        ]
    )
]


@pytest.fixture()
def keep_alive_behaviour_app():
    yield apps


def test_keep_alive_behaviour_app(keep_alive_behaviour_app):
    def _is_chunked(r, d):
        headers = r.getheaders()
        if not any(f == "Transfer-Encoding" and v == "chunked" for f, v in headers):
            print(f"Missing transfer-encoding header: {headers}")
            suffix = b"0\r\n\r\n"
            if not d.endswith(suffix):
                print(
                    f"Missing transfer-encoding header and not finalizing with: {suffix}"
                )
                return False

        return True

    def _tinker_request(app):
        if app.http_minor == 0:
            req = b"GET / HTTP/1.0\r\n"
            if app.want_keep_alive:
                req += b"Connection: Keep-Alive\r\n"
        else:
            req = b"GET / HTTP/1.1\r\n"
            if not app.want_keep_alive:
                req += b"Connection: close\r\n"
        req += b"\r\n"
        return req

    def send_request(app, data):
        app.conn.send(data)

    def get_response(app):
        app.conn._HTTPConnection__state = httplib._CS_REQ_SENT
        return app.conn.getresponse()

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

    for app in keep_alive_behaviour_app:
        p = None
        try:
            p = _run_app(app, reuse_port=True)

            send_request(app, _tinker_request(app))
            response = get_response(app)

            body = response.read()

            if app.expect_chunked and not response.chunked:
                raise Fail("Response unexpectedly not chunked")
            if (
                app.expect_chunked
                and response.chunked
                and not _is_chunked(response, body)
            ):
                raise Fail("Response unexpectedly not with chunked formatting")

            if app.raise_error:
                if response.status != 500 or body:
                    raise Fail("Expected 500 Internal Server Error")
            else:
                if response.status != 200:
                    raise Fail("Status is %d, expected 200", response.status)

            if still_alive(app.conn.sock):
                if not app.expect_keep_alive:
                    raise Fail("Expected connection not to be kept-alive")
                app.request_count -= 1
                if app.request_count:
                    if app.request_count == 1:
                        # Send Connection: close on last request
                        app.want_keep_alive = False
                        app.expect_keep_alive = False
                        app.expect_chunked = False
                    send_request(app, _tinker_request(app))
                else:
                    if still_alive(app.conn.sock):
                        raise Fail("Connection still alive")
            else:
                if app.expect_keep_alive:
                    raise Fail("Expected connection to be kept-alive")

            if not app.raise_error and body != app.body:
                raise Fail("Different bodies:\n%s\n%s" % (body, app.body))
        finally:
            os.kill(p.pid, signal.SIGKILL) if p is not None else None
            time.sleep(0.1)  # Should be enough for the server to stop
