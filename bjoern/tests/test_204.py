import os
import signal
import time
from wsgiref.validate import validator

import pytest
from bjoern.tests.conftest import _run_app


@pytest.fixture()
def wsgi_204_app():
    @validator
    def app(e, s):
        s("204 no content", [])
        return []

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_wsgi_204_app(wsgi_204_app, client):
    response = client.get("/")
    assert response.status_code == 204
    assert response.reason == "no content"
    assert response.content == b""
