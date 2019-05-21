import os
import signal
import time

import pytest
from bjoern.tests.conftest import _run_app


@pytest.fixture()
def wsgi_upgrade_app():
    def app(e, s):
        s("200 ok", [])
        return []

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_wsgi_upgrade_app(wsgi_upgrade_app, client):
    response = client.get(
        "/",
        headers={
            "Connection": "Upgrade",
            "Upgrade": "WebSocket",
            "Host": "example.com",
            "Origin": "http://example.com",
            "WebSocket-Protocol": "sample",
        },
    )
    assert response.status_code == 200
    assert response.reason == "ok"
    assert response.content == b""
