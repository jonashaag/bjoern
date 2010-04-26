# TODO: If it isn't a too great efford, rewrite this in C.
from _bjoern import add_route, run

class WSGIError(Exception):
    pass

class Response(object):
    """
    The `Response` type functions as both the `environment` dictionary and
    `start_response` callback specified in PEP 333[1].

    It is used like this (simplified)::

        request = Request(dict_with_wsgi_environment_variables())
        wsgi_app(request.environ, request.start_response)

    [1] http://www.python.org/dev/peps/pep-0333/#the-start-response-callable
    """
    response_headers_sent = False
    response_status = None
    response_headers = None

    def __init__(self, environ):
        self.environ = environ

    def start_response(self, response_status, response_headers, exc_info=None):
        """
        Implementation of the WSGI 1.0 `start_response` callback[2].

        [2] http://www.python.org/dev/peps/pep-0333/#the-start-response-callable
        """
        if self.response_headers_sent:
            if exc_info:
                try:
                    raise exc_info[0], exc_info[1], exc_info[2]
                finally:
                    # clear the `exc_info` variable to avoid circular references
                    # (as recommended in PEP 333)
                    exc_info = None
            else:
                raise WSGIError("`start_response` called multiple times without"
                                " `exc_info`. Forbidden according to PEP333.")
        else:
            self.response_status  = response_status
            self.response_headers = response_headers
