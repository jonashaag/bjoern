import os
import signal
import time

import pytest
from bottle import Bottle, request
from bjoern.tests.conftest import _run_app


@pytest.fixture()
def bottle_app():
    app = Bottle()

    @app.get("/a/b/c")
    def hello():
        return f"Hello, World! {dict(request.params)}"

    @app.post("/a/b/c")
    def hello():
        return f"Hello, World! {dict(request.params)} {dict(request.forms)}"

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_bottle_app(bottle_app, client):
    response = client.get("/a/b/c?k=v&k2=v2'")
    assert response.status_code == 200
    assert response.reason == "OK"
    assert response.content == b"Hello, World! {'k': 'v', 'k2': \"v2'\"}"
    response = client.post("/a/b/c?k=v&k2=v2'", data={"k3": "v3", "k4": "v4"})
    assert response.status_code == 200
    assert response.reason == "OK"
    assert (
        response.content == b"Hello, World! "
        b"{'k': 'v', 'k2': \"v2'\", 'k3': 'v3', 'k4': 'v4'}"
        b" {'k3': 'v3', 'k4': 'v4'}"
    )
