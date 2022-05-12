import os
import time
import sys
import subprocess
import multiprocessing
from collections import defaultdict

try:
    from http import client as httplib
except ImportError:  # Py 2
    import httplib


N_PROCESSES = min(3, multiprocessing.cpu_count())
N_REQUESTS_PER_PROCESS = 100
N_REQUESTS = N_REQUESTS_PER_PROCESS * N_PROCESSES


def cmd_test():
    processes = [subprocess.Popen([sys.executable, __file__, "app"])
                for _ in range(N_PROCESSES)]

    sleep_secs = 5 if os.getenv("CI") else 0.2
    time.sleep(sleep_secs * N_PROCESSES)


    responder_count = defaultdict(int)

    for i in range(N_REQUESTS):
        conn = httplib.HTTPConnection("localhost", 8080)
        conn.request("GET", "/")
        response = conn.getresponse().read()
        responder = response.split()[-1]
        responder_count[responder] += 1

    for proc in processes:
        proc.terminate()

    print(responder_count)
    for responder, count in responder_count.items():
        assert (count > N_REQUESTS_PER_PROCESS * 0.8 and
                count < N_REQUESTS_PER_PROCESS * 1.2), \
               "requests not correctly distributed"



def cmd_app():
    def app(environ, start_response):
        start_response('200 OK', [])
        return [b"Hello from process %d\n" % os.getpid()]

    import bjoern
    bjoern.run(app, "localhost", 8080, True)


if __name__ == '__main__':
    if "app" in sys.argv:
        cmd_app()
    else:
        cmd_test()
