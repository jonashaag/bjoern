import os
import signal
import time
from wsgiref.validate import validator

import pytest
from bjoern.tests.conftest import _run_app


@pytest.fixture()
def wsgi_signal_app():
    _n = 0

    def inc_counter(signum, frame):
        nonlocal _n
        _n += 1
        print("Increased counter to", _n)

    signal.signal(signal.SIGTERM, inc_counter)

    @validator
    def app(e, s):
        nonlocal _n
        s("200 ok", [("Content-Type", "text/plain")])
        return [b"%d times" % _n]

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_wsgi_signal(wsgi_signal_app, client):
    response = client.get("/")
    assert response.status_code == 200
    assert response.reason == "ok"
    assert response.content == b"0 times"
    os.kill(wsgi_signal_app.pid, signal.SIGTERM)
    response = client.get("/")
    assert response.status_code == 200
    assert response.reason == "ok"
    assert response.content == b"1 times"
