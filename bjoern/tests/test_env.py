import json
import os
import signal
import time

import pytest
from bjoern.tests.conftest import _run_app


@pytest.fixture()
def env_app():
    def app(env, start_response):
        start_response("200 yo", [("Content-Type", "application/json")])
        env_ = {}
        # Avoid variable and non json serializable
        env_["wsgi.input"] = f"{env['wsgi.input'].read()}"
        env_["wsgi.errors"] = "wsgi.errors" in env
        env_["wsgi.file_wrapper"] = "wsgi.file_wrapper" in env
        env_["wsgi.version"] = list(env["wsgi.version"])
        env_["SERVER_PORT"] = "SERVER_PORT" in env
        for k, v in env.items():
            if k not in env_:
                env_[k] = v
        return json.dumps(env_).encode()

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_env_app(env_app, client):
    response = client.get("/")
    assert response.status_code == 200
    assert response.reason == "yo"
    j_response = response.json()
    user_agent = j_response.pop("HTTP_USER_AGENT")
    assert "python-requests" in user_agent
    assert j_response == {
        "wsgi.input": "b''",
        "wsgi.errors": True,
        "wsgi.file_wrapper": True,
        "wsgi.version": [1, 0],
        "PATH_INFO": "/",
        "HTTP_HOST": "127.0.0.1:8080",
        "HTTP_ACCEPT_ENCODING": "gzip, deflate",
        "HTTP_ACCEPT": "*/*",
        "HTTP_CONNECTION": "keep-alive",
        "SERVER_PROTOCOL": "HTTP/1.1",
        "REQUEST_METHOD": "GET",
        "REMOTE_ADDR": "127.0.0.1",
        "SCRIPT_NAME": "",
        "wsgi.url_scheme": "http",
        "wsgi.multithread": False,
        "wsgi.multiprocess": True,
        "wsgi.run_once": False,
        "SERVER_NAME": "127.0.0.1",
        "SERVER_PORT": True,
    }
