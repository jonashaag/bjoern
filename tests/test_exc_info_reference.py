alist = []

def app(env, start_response):
    '''
    print 'foo!'
    print type(start_response)
    print dir(start_response)
    c = start_response.__class__
    try:
        c()
    except:
        import traceback; traceback.print_exc()
    try:
        class x(c): pass
    except:
        import traceback; traceback.print_exc()
    '''
    start_response('200 alright', [])
    try:
        a
    except:
        import sys
        x = sys.exc_info()
        start_response('500 error', alist, x)
    return [b'hello']

import bjoern
bjoern.run(app, '0.0.0.0', 8080)
