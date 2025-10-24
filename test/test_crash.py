import sys
import traceback
import calcimp

try:
    print('Testing calcimp...')
    result = calcimp.calcimp('sample/test.men', max_freq=100.0, step_freq=10.0)
    print('Success! Result shape:', len(result))
    print('Frequencies shape:', len(result[0]))
except Exception as e:
    print('ERROR:', e)
    traceback.print_exc()
    sys.exit(1)

