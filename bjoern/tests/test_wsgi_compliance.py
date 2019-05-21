import os
import signal
import time
from wsgiref.validate import validator

import pytest

from bjoern.tests.conftest import _run_app


@pytest.fixture()
def wsgi_compliance_app():
    @validator
    def app(environ, start_response):
        start_response("200 OK", [("Content-Type", "text/plain")])
        return [b"Hello, World!"]

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_wsgi_compliance_app(wsgi_compliance_app, client):
    response = client.get("/")
    assert response.status_code == 200
    assert response.reason == "OK"
    assert response.content == b"Hello, World!"
