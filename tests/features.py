import bjoern

if __name__ == '__main__':
    for k, v in bjoern._bjoern.features.iteritems():
        print('{}: {}'.format(k, v))

