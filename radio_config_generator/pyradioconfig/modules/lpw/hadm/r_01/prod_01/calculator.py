from pyradioconfig.modules.lpw.hadm.r_01.prod_01.calculations.calc_hadm import CalcHadmMisc
from enum import Enum
from pycalcmodel.core.variable import ModelVariableFormat, CreateModelVariableEnum
from pyradioconfig.modules.lpw.hadm.r_01.prod_01.reg_fields import HadmRegFields
from pyradioconfig.modules.lpw.hadm.r_01.prod_01.protected_reg_fields import HadmProtectedRegFields

class CalcHadm_r01(CalcHadmMisc):

    def __init__(self, peripheral_name='HADM'):
        self._reg_field_list = HadmRegFields().reg_field_list
        self._protected_reg_field_list = HadmProtectedRegFields().reg_field_list
        super().__init__(peripheral_name)

    def buildVariables(self, model):
        super().buildVariables(model)
        hadm_enable = self._addModelVariable(model, 'hadm_enable', Enum, ModelVariableFormat.DECIMAL, units='',
                               desc='Enables HADM settings')

        HadmEnableEnum = [
            ['DISABLED', 0, 'HADM Disabled'],
            ['ENABLED', 1, 'HADM Enabled'],
        ]

        hadm_enable.var_enum = CreateModelVariableEnum(
            'HADMEnableEnum',
            'HADM Enable/Disable Selection',
            HadmEnableEnum
        )

        self._addModelVariable(model, 'hadm_do_not_care', bool, ModelVariableFormat.BINARY,
                               desc='Enable dont care for HADM')

        self._addModelVariable(model,    'cs_rtt_dig_delay_ns',          int, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model,    'cs_rtt_rbs_bias_ns',           int, ModelVariableFormat.DECIMAL)