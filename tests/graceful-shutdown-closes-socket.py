import os
import signal
import time
import threading
import http.client

import bjoern

HOST = ('127.0.0.1', 9000)

already_signalled = False
requests_completed = 0


def application(environ, start_response):
    start_response('200 ok', [])
    yield b"chunk1"
    yield b"chunk2"
    yield b"chunk3"

    global requests_completed
    requests_completed += 1


def requester():
    a = http.client.HTTPConnection(*HOST)
    b = http.client.HTTPConnection(*HOST)

    # connect the socket
    a.connect()
    b.request("GET", "/")

    # initiate shutdown
    os.kill(os.getpid(), signal.SIGTERM)

    time.sleep(0.150)

    try:
        b.connect()
    except ConnectionRefusedError:
        pass
    else:
        b.request("GET", "/")


threading.Thread(target=requester).start()
bjoern.run(application, *HOST)
# only the first request should have succeeded, because upon SIGTERM the
# listener was closed.
assert requests_completed == 1
