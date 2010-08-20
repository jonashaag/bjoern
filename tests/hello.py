import bjoern

def wsgi_app(env, start_response):
    start_response('200 abc', [('Content-Length', '12')])
    #start_response('200 abc', [('Content-Length', '12')], ())
    yield 'Hello'
    yield ' World'
    yield '\n'

if 0:
    def wsgi_app(e, s):
        s('200 ok', [])
        return ['hello\n']

bjoern.run(wsgi_app, '0.0.0.0', 8080)
