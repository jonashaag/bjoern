import bjoern


def app(environ, start_response):
    start_response('200 OK', [])
    yield b'Hello world'
    yield b''

bjoern.listen(app, '0.0.0.0', 8080)
bjoern.run()
