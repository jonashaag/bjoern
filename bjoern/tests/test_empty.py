import os
import signal
import time
from wsgiref.validate import validator

import pytest
from bjoern.tests.conftest import _run_app


@pytest.fixture()
def wsgi_empty_app():
    @validator
    def app(e, s):
        s("200 ok", [("Content-Type", "text/plain")])
        return [b""]

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_wsgi_empty(wsgi_empty_app, client):
    response = client.get("/")
    assert response.status_code == 200
    assert response.reason == "ok"
    assert response.content == b""
