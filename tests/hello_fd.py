import sys
import bjoern
import socket
import os


def run():
    # creating the socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(('localhost', 8080))
    sock.listen(1024)
    fd = sock.fileno()

    pid = os.fork()

    if not pid:
        def app(env, sr):
            print env
            sr('200 ok', [])
            return 'hello'

        host = 'fd://%d' % fd
        try:
            bjoern.run(app, host)
        except KeyboardInterrupt:
            pass
    else:
        try:
            os.waitpid(pid, 0)
        except KeyboardInterrupt:
            os.kill(pid, 9)

        sock.close()

if __name__ == '__main__':
    run()
