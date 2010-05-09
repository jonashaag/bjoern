import bjoern

PAGES = dict(
    home = """
home
====""",

    foo = """
foo
===
what's up?"""
)


def route(pat):
    def wrapper(func):
        bjoern.add_route('^{pattern}$'.format(pattern=pat), func)
        return func
    return wrapper

@route('/page/[a-z]+')
def page(env, start_response):
    start_response('200 Alles ok', (('Content-type', 'text/plain'),))
    return PAGES['foo']

@route('/')
def page(env, start_response):
    start_response('200 Hi was geht', (('Content-type', 'text/plain'),))
    return PAGES['home']

print "Defining routes..."

print "Starting up server..."
bjoern.run('0.0.0.0', 8080, bjoern.Response)
