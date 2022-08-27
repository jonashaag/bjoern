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


def application(environ, start_response):
    if environ["PATH_INFO"] == "/sleep":
        print("sleeping")
        time.sleep(1)
        print("slept")

    start_response('200 ok', [])
    yield b"chunk1"
    yield b"chunk2"
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

        # spin up some connections that are going to wait a moment to get accept()ed
        threads = []
        for i in range(9):
            threads.append(threading.Thread(target=http_request))
            threads[-1].start()

        # create a signal for shutdown
        os.kill(os.getpid(), signal.SIGTERM)

        # wait for our client requests to finish
        for thread in threads:
            thread.join()

        print(sock.recv(4096))


def http_request():
    conn = http.client.HTTPConnection(*HOST)
    conn.request("GET", "/")
    conn.getresponse().read()


threading.Thread(target=requester).start()
bjoern.run(application, *HOST)
# expect 10 requests total, one from the raw socket before the signal, and
# 9 more while the loop was blocked by the sleep
assert requests_completed == 10, requests_completed
