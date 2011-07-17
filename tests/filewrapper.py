import sys
import os
import bjoern

W = {
    'callable-iterator' : lambda f, e: iter(lambda: f.read(64*1024), ''),
    'xreadlines' : lambda f, e: f,
    'filewrapper' : lambda f, env: env['wsgi.file_wrapper'](f)
}

F = len(sys.argv) > 1 and sys.argv[1] or 'README.rst'
W = len(sys.argv) > 2 and W[sys.argv[2]] or W['filewrapper']

def app(env, start_response):
    f = open(F)
    wrapped = W(f, env)
    start_response('200 ok', [('Content-Length', str(os.path.getsize(F)))])
    return wrapped

import bjoern
bjoern.run(app, '0.0.0.0', 8080)
