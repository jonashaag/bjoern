import bjoern

def _gen(environ):
    for key, value in environ.iteritems():
        yield "{key} = {value}".format(key=key.ljust(30), value=value)

def wsgi_app(environ, start_response):
    return ''
    start_response(200, {'Content-type' : 'text/plain'})
    return '\n'.join(_gen(environ))

bjoern.run(wsgi_app)
