from bjoern import run


def serve_paste(app, global_conf, **kw):
    """ A handler for PasteDeploy-compatible runners.

    Sample .ini configuration:
    
     [server:main]
     use = egg:bjoern#main
     host = 127.0.0.1
     port = 8080
     reuse_port = True
    
    If no host is specified bjoern will bind to all IPv4 IPs (0.0.0.0)
    If no port is specified, bjoern will use a random port
    reuse_port defaults to False
    """
    # Convert the values from the .ini file to something bjoern can work with
    host = kw.get('host', '')
    port = int(kw.get('port', '0')) or False
    if kw.get('reuse_port', '').lower() in ('1', 'true'):
        reuse_port = True
    else:
        reuse_port = False

    run(app, host, port=port, reuse_port=reuse_port)
    return 0
