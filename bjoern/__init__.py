import os
import subprocess

__version__ = "4.0.0rc3"
MAX_LISTEN_BACKLOG = int(
    subprocess.run(["cat", "/proc/sys/net/core/somaxconn"], stdout=subprocess.PIPE)
    .stdout.decode()
    .splitlines()[0]
)
DEFAULT_LISTEN_BACKLOG = MAX_LISTEN_BACKLOG // 2

DEFAULT_TCP_KEEPALIVE = bool(int(os.environ.get("BJOERN_SOCKET_TCP_KEEPALIVE", 1)))
DEFAULT_TCP_NODELAY = bool(int(os.environ.get("BJOERN_SOCKET_TCP_NODELAY", 1)))

DEFAULT_MAX_BODY_LEN = int(os.environ.get("BJOERN_SOCKET_MAX_BODY_LEN", 10490000000))

DEFAULT_MAX_FIELD_LEN = int(
    os.environ.get("BJOERN_SOCKET_MAX_FIELD_LEN", 8091)
)  # 10Mebibytes
DEFAULT_MAX_HEADER_FIELDS = int(os.environ.get("BJOERN_SOCKET_HEADER_FIELDS", 128))


def run(
    wsgi_app,
    host,
    port=None,
    reuse_port=False,
    listen_backlog=DEFAULT_LISTEN_BACKLOG,
    tcp_keepalive=DEFAULT_TCP_KEEPALIVE,
    tcp_nodelay=DEFAULT_TCP_NODELAY,
    fileno=None,
    max_body_len=DEFAULT_MAX_BODY_LEN,
    max_header_fields=DEFAULT_MAX_HEADER_FIELDS,
    max_header_field_len=DEFAULT_MAX_FIELD_LEN,
):
    from .server import run

    run(
        wsgi_app,
        host,
        port=port,
        reuse_port=reuse_port,
        listen_backlog=listen_backlog,
        tcp_keepalive=tcp_keepalive,
        tcp_nodelay=tcp_nodelay,
        fileno=fileno,
        max_body_len=max_body_len,
        max_header_fields=max_header_fields,
        max_header_field_len=max_header_field_len,
    )


def stop():
    from .server import stop

    stop()
