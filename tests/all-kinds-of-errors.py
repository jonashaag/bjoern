import random

def invalid_header_type(environ, start_response):
    start_response('200 ok', None)
    return ['yo']

def invalid_header_tuple(environ, start_response):
    tuples = {1: (), 2: ('a', 'b', 'c'), 3: ('a',)}
    start_response('200 ok', [tuples[random.randint(1, 3)]])
    return ['yo']

def invalid_header_tuple_item(environ, start_response):
    start_response('200 ok', (object(), object()))
    return ['yo']

apps = [invalid_header_tuple_item, invalid_header_tuple, invalid_header_type]

def randomizer(*args, **kwargs):
    return random.choice(apps)(*args, **kwargs)

import bjoern
bjoern.run(randomizer, '0.0.0.0', 8080)
