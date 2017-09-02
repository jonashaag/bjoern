#!/usr/bin/env python
# -*- coding: utf-8 -*-

from time import time
import bjoern

def start():
    def return_tuple(environ, start_response):
        start_response('200 OK', [('Content-Type','text/plain')])
        print('tuple')
        return (b'Hello,', b" it's me, ", b'Bob!')

    def return_huge_answer(environ, start_response):
        start_response('200 OK', [('Content-Type','text/plain')])
        return [b'x'*(1024*1024)]

    def return_404(environ, start_response):
        start_response('404 Not Found', [('Content-Type','text/plain')])
        return b"URL %s not found" % (environ.get('PATH_INFO', 'UNKNOWN').encode())

    dispatch = {
        '/tuple': return_tuple,
        '/huge': return_huge_answer,
    }

    def choose(environ, start_response):
        return dispatch.get(environ.get('PATH_INFO'), return_404)(environ, start_response)
    bjoern.run(choose, '0.0.0.0', 8080)

if __name__=="__main__":
    start()
