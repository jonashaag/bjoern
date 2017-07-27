import bjoern
import os, signal


NUM_WORKERS = 8
worker_pids = []


def app(environ, start_response):
    start_response('200 OK', [])
    yield b'Hello world from worker %d' % os.getpid()


bjoern.listen(app, '0.0.0.0', 8080)
for _ in range(NUM_WORKERS):
    pid = os.fork()
    if pid > 0:
        # in master
        worker_pids.append(pid)
    elif pid == 0:
        # in worker
        try:
            bjoern.run()
        except KeyboardInterrupt:
            pass
        exit()

try:
    for _ in range(NUM_WORKERS):
        os.wait()
except KeyboardInterrupt:
    for pid in worker_pids:
        os.kill(pid, signal.SIGINT)
