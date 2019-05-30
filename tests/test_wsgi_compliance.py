#!/usr/bin/env python

import os
import time

from multiprocessing import Process
from wsgiref.validate import validator

try:
    from http import client as httplib
except ImportError:  # Py 2
    import httplib

import bjoern

@validator
def _app(environ, start_response):
    start_response("200 OK", [("Content-Type", "text/plain")])
    return [b"Hello World"]

def _start_server():
    bjoern.run(_app, 'localhost', 8080)

def test_compliance():
    p = Process(target=_start_server)
    p.start()

    time.sleep(3)  # Should be enough for the server to start
    try:
        h = httplib.HTTPConnection('localhost', 8080)
        h.request("GET", "/")
        response = h.getresponse()
    finally:
        p.terminate()
    
    assert response.reason == "OK"


if __name__ == "__main__":
    try:
        test_compliance()
    except AssertionError:
        raise SystemExit("Test failed")
    else:
        print("Test successful")
