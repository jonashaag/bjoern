import logging
import requests
import time
import timeit
import sys
import signal

sleep_time = 2
port = int(sys.argv[1]) if len(sys.argv) == 2 else 8081
host = f"http://localhost:{port}/"

print(f"Making requests on {host}")

s = requests.Session()
number_of_calls = 10
server_pids = {}

def shutdown(sig, frame):
    logging.warning(f"Requests {server_pids}")
    sys.exit()

signal.signal(signal.SIGINT, shutdown)

request_no = 0
error_request_no = 0
max_duration = 0
min_duration = 9999
errors = {}

def http_call():
    global request_no
    global error_request_no
    r = s.get(host)
    server_pids[r.text] = server_pids.get(r.text, 0) + 1
    if r.status_code != 200:
        error_request_no += 1
        raise Exception(r)
    else:
        request_no += 1

while True:
    t = timeit.timeit(http_call, number=number_of_calls)

    max_duration = max(max_duration, t/number_of_calls)
    min_duration = min(min_duration, t/number_of_calls)
    sys.stdout.write("\r{0}".format(
        f"r: {request_no:10}, er: {error_request_no:5}, min: {min_duration:2.6}s, max: {max_duration:2.6}s, pids {server_pids}"))
    sys.stdout.flush()
