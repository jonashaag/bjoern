import sys
from hello import  *

if __name__ == '__main__':
    try:
        host = sys.argv[1]
    except IndexError:
        host = 'hello_unix'
    host = 'unix:' + host
    bjoern.run(wsgi_app, host)
