from pyradioconfig.calculator_model_framework.interfaces.icalculator import ICalculator
from pycalcmodel.core.variable import ModelVariableFormat, CreateModelVariableEnum
from enum import Enum

class CalcSidewalkNerio(ICalculator):

    def buildVariables(self, model):
        var = self._addModelVariable(model, 'sidewalk_mode', Enum, ModelVariableFormat.DECIMAL,
                                     'Sidewalk Mode')
        member_data = [
            ['FSK_50kbps', 0, '2FSK 50kbps mi=1.0'],
        ]
        var.var_enum = CreateModelVariableEnum(
            'SidewalkModeEnum',
            'List of supported Sidewalk Modes',
            member_data)

