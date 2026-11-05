'''
Created on Mar 22, 2013

@author: sesuskic
'''
import sys

__all__ = ["trueround"]

def _trueround(number, places):
    '''
    trueround(number, places)

    example:

        >>> trueround(2.55, 1) == 2.6
        True

    uses standard functions with no import to give "normal" behavior to 
    rounding so that trueround(2.5) == 3, trueround(3.5) == 4, 
    trueround(4.5) == 5, etc. Use with caution, however. This still has 
    the same problem with floating point math. The return object will 
    be type int if places=0 or a float if places=>1.

    number is the floating point number needed rounding

    places is the number of decimal places to round to with '0' as the
        default which will actually return our interger. Otherwise, a
        floating point will be returned to the given decimal place.

    Note:   Use trueround_precision() if true precision with
            floats is needed

    GPL 2.0
    copywrite by Narnie Harshoe <signupnarnie@gmail.com>
    '''
    place = 10**(places)
    rounded = (int(number*place + 0.5if number>=0 else -0.5))/place
    if rounded == int(rounded):
        rounded = int(rounded)
    return rounded


def trueround(number, places=0):
    if sys.version_info[0] == 3:
        return _trueround(number, places)
    else:
        return round(number)
        
