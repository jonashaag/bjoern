import os
import signal
import socket
import time
import threading
import http.client

import bjoern

HOST = ('127.0.0.1', 9000)

already_signalled = False
requests_completed = 0
error = False


def application(environ, start_response):
    start_response('200 ok', [])
    yield b"chunk1"
    yield b"chunk2"
    yield b"chunk3"

    global requests_completed
    requests_completed += 1


def requester():
    with socket.socket() as sock:
        sock.connect(HOST)
        # create an active connection by sending an incomplete request.
        sock.sendall(b"GET / HTTP/1.1\r\n")
        # tell bjoern to shutdown
        os.kill(os.getpid(), signal.SIGTERM)

        time.sleep(31)

    with socket.socket() as sock:
        try:
            sock.connect(HOST)
        except ConnectionRefusedError:
            pass
        else:
            print("listener socket was still open, did the server shutdown?")
            global error
            error = True


def http_request():
    conn = http.client.HTTPConnection(*HOST)
    conn.request("GET", "/")
    conn.getresponse().read()


t = threading.Thread(target=requester)
t.start()
s = time.time()
bjoern.run(application, *HOST)
# expect that we gave up after 30 seconds and just shutdown
assert requests_completed == 0, requests_completed

print("server shutdown after {} seconds".format(time.time() - s))

t.join()
print("client thread shutdown after {} seconds".format(time.time() - s))
assert not error
