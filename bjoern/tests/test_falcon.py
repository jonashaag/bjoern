import json
import os
import signal
import time

import falcon
import pytest
from bjoern.tests.conftest import _run_app


@pytest.fixture()
def falcon_app():
    class AppResource:
        def on_get(self, request, response):
            k = request.get_param("k")
            k2 = request.get_param("k2")

            response.status_code = 200
            response.content_type = "application/json"
            response.body = json.dumps({"k": k, "k2": k2})

        def on_post(self, request, response):
            k = request.get_param("k")
            k2 = request.get_param("k2")
            k3 = request.get_param("k3")
            k4 = request.get_param("k4")

            response.status_code = 200
            response.content_type = "application/json"
            response.body = json.dumps({"k": k, "k2": k2, "k3": k3, "k4": k4})

    app = falcon.API()
    app.req_options.auto_parse_form_urlencoded = True

    resource = AppResource()

    app.add_route("/a/b/c", resource)

    p = _run_app(app)
    try:
        yield p
    finally:
        os.kill(p.pid, signal.SIGKILL)
        time.sleep(1)  # Should be enough for the server to stop


def test_falcon_app(falcon_app, client):
    response = client.get("/a/b/c?k=v&k2=v2")
    assert response.status_code == 200
    assert response.reason == "OK"
    assert response.json() == {"k": "v", "k2": "v2"}
    response = client.post("/a/b/c?k=v&k2=v2", data={"k3": "v3", "k4": b"v4"})
    assert response.status_code == 200
    assert response.reason == "OK"
    assert response.json() == {"k": "v", "k2": "v2", "k3": "v3", "k4": "v4"}
