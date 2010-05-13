import bjoern
import os

def listdir(d):
    d = os.path.expanduser(d)
    html = '<html><head><title>Listing of %s</title><body><ul>' % d
    for file_ in os.listdir(d):
        html += '<li><a href="%s">%s' % (file_, file_)
    return html

PAGES = dict(
    home = """
home
====""",

    foo = """
foo
===
what's up?"""
)

@bjoern.route('/page/[a-z]+')
def page(env, start_response):
    start_response('200 Alles ok', (('Content-Type', 'text/plain'),))
    return PAGES['foo']

@bjoern.route('/files/.*')
def files(env, start_response):
    file_= env['PATH_INFO'][len('/files/'):]
    if file_:
        return open(os.path.join(os.path.expanduser('~/movies'), file_))
    start_response('200 Alles ok', (('Content-Type', 'text/html'),))
    return listdir('~/movies')

@bjoern.route('/')
def home(env, start_response):
    start_response('200 Hi was geht', (('Content-type', 'text/plain'),))
    return PAGES['home']

print "Defining routes..."

print "Starting up server..."
bjoern.run('0.0.0.0', 8080, bjoern.Response)
