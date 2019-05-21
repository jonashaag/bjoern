import subprocess
import sys
import time
from pathlib import Path

import pytest
from flask import Flask, request


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

    return app


app = flask_app()


@pytest.fixture()
def run_app_gunicorn():
    executable = Path(sys.executable).parent / "gunicorn"
    p = subprocess.Popen(
        [
            executable,
            "bjoern.tests.test_gunicorn:app",
            "--access-logfile=-",
            "--workers",
            "1",
            "--threads",
            "2",
            "--bind",
            "localhost:8080",
            "--worker-class",
            "bjoern.gworker.BjoernWorker",
        ]
    )
    time.sleep(2)  # Should be enough for the server to start

    try:
        yield p
    finally:
        subprocess.run(["killall", "-9", "gunicorn"])
        time.sleep(0.5)


def test_flask_gunicorn_app(run_app_gunicorn, client):
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
