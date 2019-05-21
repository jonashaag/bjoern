import os
import random
import signal
import time

import pytest
from bjoern.tests.conftest import _run_app


@pytest.fixture()
def errors_apps():
    def invalid_header_type(environ, start_response):
        start_response("200 ok", None)
        return ["yo"]

    def invalid_header_tuple(environ, start_response):
        tuples = {1: (), 2: ("a", "b", "c"), 3: ("a",)}
        start_response("200 ok", [tuples[random.randint(1, 3)]])
        return ["yo"]

    def invalid_header_tuple_item(environ, start_response):
        start_response("200 ok", (object(), object()))
        return ["yo"]

    return [invalid_header_type, invalid_header_tuple, invalid_header_tuple_item]


def test_errors(errors_apps, client):
    for app in errors_apps:
        p = _run_app(app)
        try:
            resp = client.get("/")
            assert resp.status_code == 500
        finally:
            os.kill(p.pid, signal.SIGKILL)
            time.sleep(0.1)  # Should be enough for the server to stop
