import os
import signal
import time

import pytest
from bjoern.tests.conftest import _run_app

N = 1024
CHUNK = b"a" * 1024 * 1024
DATA_LEN = N * len(CHUNK)


class _iter(object):
    def __iter__(self):
        for i in range(N):
            yield CHUNK


@pytest.fixture()
def huge_app_out():
    def app(e, s):
        s("200 ok", [("Content-Length", str(DATA_LEN))])
        return _iter()

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_huge_out_app(huge_app_out, client):
    response = client.get("/")
    assert response.status_code == 200
    assert response.reason == "ok"
    assert response.headers["Content-Length"] == str(str(DATA_LEN))
