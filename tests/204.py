def app(e, s):
    s('204 no content', [])
    return b''

import bjoern
bjoern.run(app, '0.0.0.0', 8081)
