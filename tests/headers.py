import pprint
import bjoern

def app(env, start_response):
    pprint.pprint(env)
    start_response('200 yo', [])
    return []

bjoern.run(app, '0.0.0.0', 8080)
