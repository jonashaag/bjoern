import bjoern

SERVER_PORT = 9998
SERVER_BOOT_DURATION = 0.1

def run_server(app, host, port):
    if bjoern.HAVE_ROUTING:
        bjoern.route('.*')(app)
        bjoern.run(host, port)
    else:
        bjoern.run(app, host, port)
