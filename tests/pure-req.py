import sys
import socket

conn = socket.create_connection(('0.0.0.0', 8080))
msgs = [
    # 0 Keep-Alive, Transfer-Encoding chunked
    'GET / HTTP/1.1\r\nConnection: Keep-Alive\r\n\r\n',

    # 1,2,3 Close, EOF "encoding"
    'GET / HTTP/1.1\r\n\r\n',
    'GET / HTTP/1.1\r\nConnection: close\r\n\r\n',
    'GET / HTTP/1.0\r\nConnection: Keep-Alive\r\n\r\n',

    # 4 Bad Request
    'GET /%20%20% HTTP/1.1\r\n\r\n',

    # 5 Bug #14
    'GET /%20abc HTTP/1.0\r\n\r\n',

    # 6 Content-{Length, Type}
    'GET / HTTP/1.0\r\nContent-Length: 11\r\n'
    'Content-Type: text/blah\r\nContent-Fype: bla\r\n'
    'Content-Tength: bla\r\n\r\nhello world',

    # 7 POST memory leak
    'POST / HTTP/1.0\r\nContent-Length: 1000\r\n\r\n%s' % ('a'*1000),

    # 8,9 CVE-2015-0219
    'GET / HTTP/1.1\r\nFoo_Bar: bad\r\n\r\n',
    'GET / HTTP/1.1\r\nFoo-Bar: good\r\nFoo_Bar: bad\r\n\r\n'
]
conn.send(msgs[int(sys.argv[1])].encode())
while 1:
    data = conn.recv(100)
    if not data: break
    print(repr(data))
    if data.endswith(b'0\r\n\r\n'):
        if raw_input('new request? Y/n') == 'n':
            exit()
        conn.send(b'GET / HTTP/1.1\r\nConnection: Keep-Alive\r\n\r\n')
