import bjoern
from random import choice

def app1(env, sr):
    #print(env)
    sr('200 ok', [('Foo', 'Bar'), ('Blah', 'Blubb'), ('Spam', 'Eggs'), ('Blurg', 'asdasjdaskdasdjj asdk jaks / /a jaksdjkas jkasd jkasdj '),
                  ('asd2easdasdjaksdjdkskjkasdjka', 'oasdjkadk kasdk k k k k k ')])
    return [b'hello', b'world']

def app2(env, sr):
    #print(env)
    sr('200 ok', [])
    return b'hello'

def app3(env, sr):
    #print(env)
    sr('200 abc', [('Content-Length', '12')])
    yield b'Hello'
    yield b' World'
    yield b'\n'

def app4(e, sr):
    sr('200 ok', [])
    return [b'hello there ... \n']

apps = (app1, app2, app3, app4)

def wsgi_app(env, start_response):
    return choice(apps)(env, start_response)

if __name__ == '__main__':
    bjoern.run(wsgi_app, '0.0.0.0', 8080)
