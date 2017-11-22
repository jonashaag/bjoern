# encoding: utf-8
import pprint
import bjoern

def app(env, start_response):
    start_response('200 yo', [('Content-Type', 'text/plain'), ('Ü', 'ä'), ('a'*1000, 'b'*1000)])
    return [b'foo', b'bar']

bjoern.run(app, '0.0.0.0', 8080)
