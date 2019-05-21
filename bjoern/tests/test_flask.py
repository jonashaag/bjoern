import os
import signal
import time

import pytest
from flask import Flask, request
from bjoern.tests.conftest import _run_app


@pytest.fixture()
def flask_app():
    app = Flask(__name__)

    @app.route("/a/b/c", methods=("GET", "POST"))
    def hello_world():
        return (
            f"Hello, World! Args:"
            f" {request.args.get('k'), request.args.get('k2'), dict(request.form)}"
        )

    @app.route("/image", methods=("POST",))
    def image():
        image = request.form["image"]
        return f"{image}"

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_flask_app(flask_app, client):
    response = client.get("/a/b/c?k=v&k2=v2")
    assert response.status_code == 200
    assert response.reason == "OK"
    assert response.content == b"Hello, World! Args: ('v', 'v2', {})"
    response = client.post("/a/b/c?k=v&k2=v2", data={"k3": "v3", "k4": b"v4"})
    assert response.status_code == 200
    assert response.reason == "OK"
    assert (
        response.content == b"Hello, World! Args: ('v', 'v2', {'k3': 'v3', 'k4': 'v4'})"
    )
