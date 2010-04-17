import bjoern

def _gen(environ):
    for key, value in environ.iteritems():
        yield "{key} = {value}".format(key=key.ljust(30), value=value)

def wsgi_app(environ, start_response):
    start_response('200 Alles ok', {'A' : '42'})
    return ['a'*1000]
    s = '\n'.join(_gen(environ))
    return s

bjoern.run(wsgi_app)
