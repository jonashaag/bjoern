import bjoern


def app(environ, start_response):
    start_response('200 OK', [])
    yield 'Hello world'
    yield ''

bjoern.listen(app, '0.0.0.0', 8080)
bjoern.run()
