N = 1024
CHUNK = b'a' * 1024
DATA_LEN = N * len(CHUNK)

class _iter(object):
    def __iter__(self):
        for i in range(N):
            yield CHUNK

def app(e, s):
    s('200 ok', [('Content-Length', str(DATA_LEN))])
    return _iter()

import bjoern
bjoern.run(app, '0.0.0.0', 8080)
