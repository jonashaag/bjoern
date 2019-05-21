import os
import signal
import time

import pytest
from bjoern.tests.conftest import _run_app

data = b"a" * 1024 * 1024
DATA_LEN = len(data)


@pytest.fixture()
def huge_app_in():
    def app(e, s):
        s("200 ok", [])
        return []

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_huge_app_in(huge_app_in, client):
    response = client.post("/image", data={"data": data})
    assert response.status_code == 200
    assert response.reason == "ok"
