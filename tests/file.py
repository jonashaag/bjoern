import os

FILES = {
    'small' : 888,
    'big' : 88888
}

for name, size in FILES.items():
    new_name = '/tmp/bjoern.%s.tmp' % name
    with open(new_name, 'wb') as f:
        f.write(os.urandom(size))
    FILES[name] = new_name

def app(env, start_response):
    start_response('200 ok', [])
    if env['PATH_INFO'].startswith('/big'):
        return open(FILES['big'], 'rb')
    return open(FILES['small'], 'rb')

import bjoern
bjoern.run(app, '0.0.0.0', 8080)
