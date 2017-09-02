"""The sample slow + fast client.

The client can work fine if server is localhost.  Otherwise time_request
client usually takes the same time as long_request.
"""

import sys
import socket
import time
import threading

if len(sys.argv) > 2:
    print("usage: %s [host[:port]]" % sys.argv[0])
    sys.exit(1)

hostname = 'localhost'
port = 8080
if len(sys.argv) == 2:
    if ':' in sys.argv[1]:
        hostname, port = sys.argv[1].split(':')
        port = int(port)
    else:
        hostname = sys.argv[1]
headers = '\r\nHost: %s\r\n\r\n' % hostname

def timed(func):
    def wrapper(*args, **kwargs):
        start = time.time()
        try:
            return func(*args, **kwargs)
        finally:
            print('%s took %.2f sec' % (repr(func), time.time() - start))
    return wrapper

lock = threading.Lock()
@timed
def long_request():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((hostname, port))
    s.send('GET /huge HTTP/1.0' + headers)
    for i in range(1):
        s.recv(80)
        time.sleep(0.1)
    lock.release()
    print('release')
    for i in range(10):
        s.recv(80)
        time.sleep(0.1)
    s.close()

@timed
def time_request():
    lock.acquire()
    print('acquire')
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((hostname, port))
    s.send('GET /tuple HTTP/1.0' + headers)
    s.recv(900)
    s.close()

lock.acquire()
fl = threading.Thread(target=long_request)
tl = threading.Thread(target=time_request)
fl.start()
tl.start()
tl.join()
fl.join()
