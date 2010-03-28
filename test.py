import time
import bjoern
import thread
import procname
procname.setprocname('bjoernpy')

def rq_cb(b_url, b_query, b_fragment, b_path, b_headers, b_body):
    return
    print dict((k,v) for k,v in locals().iteritems() if k.startswith('b_'))


def run_bjoern():
    port = 8080
    host = "0.0.0.0"

    while True:
        try:
            print "Trying start server on {host}:{port}".format(**locals())
            bjoern.run(host, port, rq_cb)
            break
        except RuntimeError:
            port += 1

#thread.start_new_thread(run_bjoern, ())
run_bjoern()

print "Yo."
