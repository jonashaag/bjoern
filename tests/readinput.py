from __future__ import print_function
import pprint
import bjoern
import sys

if 'silenceillkillyou' in sys.argv:
    def print(*args): pass

def app(env, start_response):
    print('--')
    stream = env['wsgi.input']
    print(stream)
    print(dir(stream))
    print(repr(stream.read(2)))
    print(repr(stream.read(10)))
    print(repr(stream.read(10)))
    print(repr(stream.read(10)))
    print(repr(stream.read(10)))
    print(repr(stream.read(10)))
    start_response('200 yo', [])
    return []

bjoern.run(app, '0.0.0.0', 8080)
