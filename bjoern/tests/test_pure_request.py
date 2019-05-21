import os
import signal
import socket
import time

import pytest
from bjoern.tests.conftest import _run_app


@pytest.fixture()
def pure_app():
    def app(environ, start_response):
        start_response("200 OK", [])
        return [b""]

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(0.1)  # Should be enough for the server to stop


def test_pure_request(pure_app):
    conn = socket.create_connection(("127.0.0.1", 8080))
    msgs = [
        # 0 Keep-Alive, Transfer-Encoding chunked
        "GET / HTTP/1.1\r\nConnection: Keep-Alive\r\n\r\n",
        # 1,2,3 Close, EOF "encoding"
        "GET / HTTP/1.1\r\n\r\n",
        "GET / HTTP/1.1\r\nConnection: close\r\n\r\n",
        "GET / HTTP/1.0\r\nConnection: Keep-Alive\r\n\r\n",
        # 4 Bad Request
        "GET /%20%20% HTTP/1.1\r\n\r\n",
        # 5 Bug #14
        "GET /%20abc HTTP/1.0\r\n\r\n",
        # 6 Content-{Length, Type}
        "GET / HTTP/1.0\r\nContent-Length: 11\r\n"
        "Content-Type: text/blah\r\nContent-Fype: bla\r\n"
        "Content-Tength: bla\r\n\r\nhello world",
        # 7 POST memory leak
        "POST / HTTP/1.0\r\nContent-Length: 1000\r\n\r\n%s" % ("a" * 1000),
        # 8,9 CVE-2015-0219
        "GET / HTTP/1.1\r\nFoo_Bar: bad\r\n\r\n",
        "GET / HTTP/1.1\r\nFoo-Bar: good\r\nFoo_Bar: bad\r\n\r\n",
    ]
    counter = 1
    for msg in msgs:
        time.sleep(0.1)
        print(f"Sending msg({counter}): {msg}")
        try:
            conn.send(msg.encode())
        except (ConnectionResetError, BrokenPipeError):
            conn = socket.create_connection(("127.0.0.1", 8080))
            conn.send(msg.encode())
        while 1:
            try:
                data = conn.recv(100)
            except BrokenPipeError:
                assert counter in (5, 6)
                break
            else:
                print(f"Received data({counter}): {data}")
                if not data:
                    break
                if data.endswith(b"0\r\n\r\n"):
                    conn.send(b"GET / HTTP/1.1\r\nConnection: Keep-Alive\r\n\r\n")
                    break
        counter += 1
    assert True
