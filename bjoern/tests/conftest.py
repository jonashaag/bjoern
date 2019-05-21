import json as _json
import time
from multiprocessing import Process

import requests

import bjoern
import pytest


class TestClient:
    base_url: str = "http://127.0.0.1:8080"

    def get(self, path="/", params=None, headers=None, **kwargs):
        _headers = {}

        if headers is not None:
            _headers.update(headers)

        if params and len(params):
            uri_params = "&".join([k + "=" + str(v) for k, v in params.items()])
            path = f"{path}?{uri_params}"

        return requests.get(f"{self.base_url}{path}", headers=_headers, **kwargs)

    def post(self, path="/", json=None, data=None, headers=None):
        headers_ = {"Content-Type": "application/x-www-form-urlencoded"}

        if json is not None:
            data = _json.dumps(json)
            headers_["Content-Type"] = "application/json"

        if headers is not None:
            headers_.update(headers)

        return requests.post(f"{self.base_url}{path}", data=data, headers=headers_)


@pytest.fixture
def client():
    return TestClient()


def _run_app(app, reuse_port=False):
    def _start_server(_app_):
        bjoern.run(_app_, "localhost", 8080, reuse_port=reuse_port)

    p = Process(target=_start_server, args=(app,))
    p.start()

    time.sleep(2)  # Should be enough for the server to start

    return p
