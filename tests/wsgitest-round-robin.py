import sys
import random
import wsgitest
import wsgitest.run

tests = wsgitest.run.find_tests([wsgitest.DEFAULT_TESTS_DIR]).tests
test = None

if len(sys.argv) > 1:
    try:
        index = int(sys.argv[1])
    except ValueError:
        module_name, test_name = sys.argv[1:]
        for module, tests_ in tests.iteritems():
            if module.__name__ == module_name:
                for test_ in tests_:
                    if test_.app.__name__ == test_name:
                        test = test_
                        break
                else:
                    continue
                break
    else:
        iterator = iter(tests.chainvalues())
        index -= 1
        while index:
            iterator.next()
            index -= 1
        test = iterator.next()

    assert test is not None
    def choose_test():
        return test.app
else:
    testlist = list(tests.chainvalues())
    def choose_test():
        return random.choice(testlist).app

def app(environ, start_response):
    app = choose_test()
    print 'Testing %s...' % app.__name__
    return app(environ, start_response)

import bjoern
bjoern.run(app, '127.0.0.1', 8080)
