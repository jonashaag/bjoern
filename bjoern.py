import os
import socket
import _bjoern


_default_instance = None
DEFAULT_LISTEN_BACKLOG = 1024


def bind_and_listen(host, port=None, reuse_port=False,
                    listen_backlog=DEFAULT_LISTEN_BACKLOG):
    if host.startswith("unix:@"):
        # Abstract UNIX socket: "unix:@foobar"
        sock = socket.socket(socket.AF_UNIX)
        sock.bind('\0' + host[6:])
    elif host.startswith("unix:"):
        # UNIX socket: "unix:/tmp/foobar.sock"
        sock = socket.socket(socket.AF_UNIX)
        sock.bind(host[5:])
    else:
        # IP socket
        sock = socket.socket(socket.AF_INET)
        # Set SO_REUSEADDR to make the IP address available for reuse
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if reuse_port:
            # Enable "receive steering" on FreeBSD and Linux >=3.9. This allows
            # multiple independent bjoerns to bind to the same port (and
            # ideally also set their CPU affinity), resulting in more efficient
            # load distribution.  https://lwn.net/Articles/542629/
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        sock.bind((host, port))

    sock.listen(listen_backlog)

    return sock


def server_run(sock, wsgi_app):
    _bjoern.server_run(sock, wsgi_app)


# Backwards compatibility API
def listen(wsgi_app, host, port=None, reuse_port=False,
           listen_backlog=DEFAULT_LISTEN_BACKLOG):
    """
    Makes bjoern listen to 'host:port' and use 'wsgi_app' as WSGI application.
    (This does not run the server mainloop.)

    'reuse_port' -- whether to set SO_REUSEPORT (if available on platform)
    'listen_backlog' -- listen backlog value (default: 1024)
    """
    global _default_instance
    if _default_instance:
        raise RuntimeError("Only one global server instance possible")
    sock = bind_and_listen(host, port, reuse_port,
                           listen_backlog=listen_backlog)
    _default_instance = (sock, wsgi_app)


def run(*args, **kwargs):
    """
    run(*args, **kwargs):
        Calls listen(*args, **kwargs) and starts the server mainloop.

    run():
        Starts the server mainloop. listen(...) has to be called before calling
        run() without arguments."
    """
    global _default_instance

    if args or kwargs:
        # Called as `bjoern.run(wsgi_app, host, ...)`
        listen(*args, **kwargs)
    else:
        # Called as `bjoern.run()`
        if not _default_instance:
            raise RuntimeError("Must call bjoern.listen(wsgi_app, host, ...) "
                               "before calling bjoern.run() without "
                               "arguments.")

    sock, wsgi_app = _default_instance
    try:
        server_run(sock, wsgi_app)
    finally:
        if sock.family == socket.AF_UNIX:
            filename = sock.getsockname()
            if filename[0] != '\0':
                os.unlink(sock.getsockname())
        sock.close()
        _default_instance = None
