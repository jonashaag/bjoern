# TODO: If it isn't a too great efford, rewrite this in C.
from _bjoern import run as _run
try:
    from _bjoern import add_route
    HAVE_ROUTING = True
except ImportError:
    # compiled without routing
    HAVE_ROUTING = False

class Response(object):
    """
    The `Response` type functions as both the `environment` dictionary and
    `start_response` callback specified in PEP 333[1].

    It is used like this (simplified)::

        request = Request(dict_with_wsgi_environment_variables())
        wsgi_app(request.environ, request.start_response)

    [1] http://www.python.org/dev/peps/pep-0333/#the-start-response-callable
    """
    def __init__(self, environ):
        self.environ = environ

    def start_response(self, response_status, response_headers, exc_info=None):
        self.response_status  = response_status
        self.response_headers = response_headers

    __call__ = start_response

if HAVE_ROUTING:
    def route(pat):
        def wrapper(func):
            add_route('^{pattern}$'.format(pattern=pat), func)
            return func
        return wrapper

    def run(host, port, response=Response):
        return _run(host, port, response)
else:
    def run(app, host, port, response=Response):
        return _run(app, host, port, response)
