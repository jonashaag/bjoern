import signal

n = 0

def app(e, s):
    s('200 ok', [])
    return b'%d times' % n


def inc_counter(*args):
    global n
    n += 1
    print('Increased counter to',n)


signal.signal(signal.SIGTERM, inc_counter)

import bjoern
bjoern.run(app, '0.0.0.0', 8080)
