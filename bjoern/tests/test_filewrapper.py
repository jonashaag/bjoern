import os
import signal
import sys
import time

import pytest
from bjoern.tests.conftest import _run_app

with open("README.md", "rb") as _f:
    readme = _f.read()


@pytest.fixture()
def wsgi_filewrapper_app():
    W = {
        "callable-iterator": lambda f, _: iter(lambda: f.read(64 * 1024), b""),
        "xreadlines": lambda f, _: f,
        "filewrapper": lambda f, env: env["wsgi.file_wrapper"](f),
        "filewrapper2": lambda f, env: env["wsgi.file_wrapper"](f, 1),
        "pseudo-file": lambda f, env: env["wsgi.file_wrapper"](PseudoFile()),
    }

    F = len(sys.argv) > 1 and sys.argv[1] or "README.md"
    W = len(sys.argv) > 2 and W[sys.argv[2]] or W["filewrapper"]

    class PseudoFile:
        def read(self, *ignored):
            return b"ab"

    def app(env, start_response):
        f = open(F, "rb")
        wrapped = W(f, env)
        start_response("200 ok", [("Content-Length", str(os.path.getsize(F)))])
        return wrapped

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_wsgi_filewrapper_app(wsgi_filewrapper_app, client):
    response = client.get("/")
    assert response.status_code == 200
    assert response.reason == "ok"
    assert response.content == readme
    assert response.headers["Content-Length"] == str(len(readme))
