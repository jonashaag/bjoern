import pprint
import bjoern

def app(env, start_response):
    pprint.pprint(env)
    print(len(env['wsgi.input'].read()))
    start_response('200 yo', [])
    return []

bjoern.run(app, '0.0.0.0', 8080)
