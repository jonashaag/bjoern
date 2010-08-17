import bjoern

SERVER_PORT = 9998
SERVER_BOOT_DURATION = 0.1

def run_server(app, host, port):
    bjoern.run(app, host, port)
