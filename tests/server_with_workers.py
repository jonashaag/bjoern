#!/usr/bin/python 

import os
import signal
import sys
import time
import bjoern

host = "0.0.0.0"
port = int(sys.argv[1]) if len(sys.argv) == 2 else 8081
workers = 3

worker_pids = []

socket = bjoern.bind_and_listen(host=host, port=port, listen_backlog=1)

requests_count = 0

print(f"http://{host}:{port} with {workers} workers")

parent_pid = os.getpid()

def shutdown(sig, frame):
    print("Wait for children to exit")
    for child_pid in worker_pids:
        print(f"killing child with pid {child_pid}.")
        os.kill(child_pid, signal.SIGINT)    

def application(environ, start_response):
    global requests_count
    requests_count += 1
    status = '200 OK'
    output = bytes(str(os.getpid()), "utf-8")
    response_headers = [('Content-type', 'text/plain'),
                        ('Content-Length', str(len(output)))]
    start_response(status, response_headers)
    return [output]

for i in range(workers):
    pid = os.fork()

    if pid > 0:
        worker_pids.append(pid)
    elif pid == 0:
        worker_pids = []
        # Run HTTP server
        print(f"Start listening for {os.getpid()}")
        try:
           bjoern.server_run(socket, application)
        except KeyboardInterrupt as _:
            print(f"{os.getpid()}: {requests_count}")
        sys.exit()

if parent_pid == os.getpid():
    signal.signal(signal.SIGINT, shutdown)

while len(worker_pids) > 0:
    child_pid, exit_status = os.wait()
    if child_pid in worker_pids:
        worker_pids.remove(child_pid)
    else:
        print(f"Child with pid {child_pid} not found in parent's process hash.")
    print(f"Child with pid {child_pid} exited")

sys.exit()
