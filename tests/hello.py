import bjoern
from random import choice

def app1(env, sr):
    sr('200 ok', [('Foo', 'Bar'), ('Blah', 'Blubb'), ('Spam', 'Eggs'), ('Blurg', 'asdasjdaskdasdjj asdk jaks / /a jaksdjkas jkasd jkasdj '),
                  ('asd2easdasdjaksdjdkskjkasdjka', 'oasdjkadk kasdk k k k k k ')])
    return ['hello', 'world']

def app2(env, sr):
    sr('200 ok', [])
    return 'hello'

def app3(env, sr):
    sr('200 abc', [('Content-Length', '12')])
    yield 'Hello'
    yield ' World'
    yield '\n'

def app4(e, s):
    s('200 ok', [])
    return ['hello\n']

apps = (app1, app2, app3, app4)

def wsgi_app(env, start_response):
    return choice(apps)(env, start_response)

bjoern.run(wsgi_app, '0.0.0.0', 8080)
