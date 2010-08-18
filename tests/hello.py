import bjoern

def wsgi_app(env, start_response):
    start_response('200 abc', [])
    yield 'Hello'
    yield ' World'
    yield '\n'

bjoern.run(wsgi_app, '0.0.0.0', 8080)
