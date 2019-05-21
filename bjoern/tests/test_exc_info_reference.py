import os
import signal
import time
from wsgiref.validate import validator

import pytest
from bjoern.tests.conftest import _run_app


@pytest.fixture()
def exec_info_reference_app():
    _alist = []

    @validator
    def app(env, start_response):
        start_response("200 alright", [("Content-Type", "text/plain")])
        try:
            a
        except:
            import sys

            x = sys.exc_info()
            start_response("500 error", _alist, x)
        return [b"hello"]

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_wsgi_exec_info(exec_info_reference_app, client):
    response = client.get("/")
    assert response.status_code == 500
    assert response.reason == "Internal Server Error"
    assert response.content == b""
