def app(e, s):
    s('200 ok', [])
    return b''

import bjoern
bjoern.run(app, '0.0.0.0', 8080)
