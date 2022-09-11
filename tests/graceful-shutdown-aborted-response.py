import os
import signal
import socket
import time
import threading
import http.client

import bjoern

HOST = ('127.0.0.1', 9000)

requests_completed = 0


def application(environ, start_response):
    start_response('200 ok', [])
    yield b"chunk1"
    yield b"chunk2"
    assert False
    yield b"chunk3"

    global requests_completed
    requests_completed += 1


def requester():
    # wait just a little to give the server a chance
    time.sleep(0.1)
    with socket.socket() as sock:
        sock.connect(HOST)
        # create an active connection by sending a request and delay receiving it
        sock.sendall(b"GET /sleep HTTP/1.1\r\n\r\n")

        # create a signal for shutdown
        os.kill(os.getpid(), signal.SIGTERM)


threading.Thread(target=requester).start()
start = time.time()
bjoern.run(application, *HOST)

# we should have zero completed requests
assert requests_completed == 0, requests_completed

# we expect aborted connections to be accounted for
print(time.time() - start)
assert time.time() - start < 30
