import math
import sys

if sys.version_info[0] == 3:
    long = int
    xrange = range
    basestring = str
    sys.maxint = 9223372036854775807
    import builtins as __builtin__

    try:
        from importlib import reload
    except ImportError:
        try:
            import imp

            reload = imp.reload
        except ImportError:
            def reload(module):
                pass



def py2round(x):
    if x >= 0.0:
        return math.floor(x + 0.5)
    else:
        return math.ceil(x - 0.5)
