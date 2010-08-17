import bjoern

def wsgi_app(env, start_response):
    start_response('200 abc', [])
    return ['Hello World']

bjoern.run(wsgi_app, '0.0.0.0', 8080)
