

"""
This module contains the parser object, that is consumed by all
parser objects to determine factory view and evaluate the activation and
visibility logic strings.
"""

__all__ = [ 'ModelParser' ]


class ModelParser(object):

    def __init__(self):
        self.ignore_act_logic = False
        self.ZZ_EVAL_VARS = {}

    def update_feature(self, name, value):
        assert isinstance(name, str)
        assert isinstance(value, bool)
        self.ZZ_EVAL_VARS['feature_' + name] = value

    def eval_act_logic(self, act_logic_str):
        """
        Return True if ignore_act_logic is set or the act_logic_str argument
        is empty; otherwise, evaluate the act_logic_str and return the result.
        """
        if self.ignore_act_logic:
            return True
        if len(act_logic_str) == 0:
            return True
        eval_str = act_logic_str
        for (op, py_op) in (('!', ' not '), ('&&', ' and '), ('||', ' or ')):
            eval_str = eval_str.replace(op, py_op)
        result = eval(eval_str, self.ZZ_EVAL_VARS)
        return result