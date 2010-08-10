import bjoern

def wsgi_app(env, start_response):
    start_response('200 abc', [])
    return []

if bjoern.HAVE_ROUTING:
    bjoern.route('.*')(wsgi_app)
    bjoern.run('0.0.0.0', 8080)
else:
    bjoern.run(wsgi_app, '0.0.0.0', 8080)
