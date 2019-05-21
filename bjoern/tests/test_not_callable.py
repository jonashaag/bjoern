import os
import signal
import time

from bjoern.tests.conftest import _run_app


def test_not_callable(client):
    p = _run_app(object())
    try:
        resp = client.get("/")
        assert resp.status_code == 500
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(0.1)  # Should be enough for the server to stop
