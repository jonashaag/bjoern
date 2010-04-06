import bjoern

def _gen():
    for key, value in environ.iteritems():
        yield "{key} = {value}".format(key=key.ljust(30), value=value)

def wsgi_app(environ, start_response):
    start_response({'Content-type' : 'text/plain'})
    s = ''.join(gen(environ))
    print len(s)
    return s

bjoern.run(wsgi_app)
