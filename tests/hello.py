import bjoern

def wsgi_app(env, start_response):
    start_response('200 ok', [])
    #yield 'a'
    #yield 'b'
    return ['hello world!\n']

if bjoern.HAVE_ROUTING:
    bjoern.route('.*')(wsgi_app)
    bjoern.run('0.0.0.0', 8080)
else:
    bjoern.run(wsgi_app, '0.0.0.0', 8080)
