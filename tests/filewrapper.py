import sys
import os
import bjoern

W = {
    'callable-iterator': lambda f, _: iter(lambda: f.read(64*1024), b''),
    'xreadlines': lambda f, _: f,
    'filewrapper': lambda f, env: env['wsgi.file_wrapper'](f),
    'filewrapper2': lambda f, env: env['wsgi.file_wrapper'](f, 1),
    'pseudo-file': lambda f, env: env['wsgi.file_wrapper'](PseudoFile())
}

F = len(sys.argv) > 1 and sys.argv[1] or 'README.rst'
W = len(sys.argv) > 2 and W[sys.argv[2]] or W['filewrapper']


class PseudoFile:
    def read(self, *ignored):
        return b'ab'

def app(env, start_response):
    f = open(F, 'rb')
    wrapped = W(f, env)
    start_response('200 ok', [('Content-Length', str(os.path.getsize(F)))])
    return wrapped

import bjoern
bjoern.run(app, '0.0.0.0', 8080)
