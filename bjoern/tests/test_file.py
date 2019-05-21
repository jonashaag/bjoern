import os
import signal
import time
from io import BytesIO

import pytest
from bjoern.tests.conftest import _run_app

FILES: dict = {"big": None, "small": None}

with open("/dev/urandom", "rb") as source:
    FILES["big"] = (BytesIO(source.read(512 * 1024 * 1024)), 512 * 1024 * 1024)
    FILES["small"] = (BytesIO(source.read(256 * 1024)), 256 * 1024)

for name, f_s in FILES.items():
    file, size = f_s
    new_name = "/opt/bjoern.%s.tmp" % name
    with open(new_name, "wb") as f:
        f.write(file.read(size))
    FILES[name] = new_name


@pytest.fixture()
def files_app():
    def app(env, start_response):
        start_response("200 ok", [])
        if env["PATH_INFO"].startswith("/big"):
            return open(FILES["big"], "rb")
        if env["PATH_INFO"].startswith("/small"):
            return open(FILES["small"], "rb")
        return []

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_files_app(files_app, client):
    response = client.get("/small")
    assert response.status_code == 200
    assert len(response.content) == 256 * 1024
    response = client.get("/big")
    assert response.status_code == 200
    assert len(response.content) == 512 * 1024 * 1024
