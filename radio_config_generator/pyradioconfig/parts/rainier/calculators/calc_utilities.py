from pyradioconfig.parts.bobcat.calculators.calc_utilities import Calc_Utilities_Bobcat
from pycalcmodel.core.variable import ModelVariableFormat

class CalcUtilitiesRainier(Calc_Utilities_Bobcat):

    def buildVariables(self, model):
        super().buildVariables(model)

        self._addModelVariable(model, 'fefilt_selected', str, ModelVariableFormat.ASCII)

    def calc_fefilt_selected(self, model):

        demod_select = model.vars.demod_select.value

        fefilt_selected = 'FEFILT0'

        model.vars.fefilt_selected.value = fefilt_selected

    def get_fefilt_actual(self, model, reg_name_str):

        fefilt_selected = model.vars.fefilt_selected.value

        reg_name_complete = fefilt_selected+'_'+reg_name_str
        reg = getattr(model.vars, reg_name_complete)

        return reg.value

    def get_fefilt_value_forced(self, model, reg_name_str):

        fefilt_selected = model.vars.fefilt_selected.value

        reg_name_complete = fefilt_selected+'_'+reg_name_str
        reg = getattr(model.vars, reg_name_complete)

        return reg.value_forced

    def write_fefilt_reg(self, model, reg_name_str, value):

        fefilt_selected = model.vars.fefilt_selected.value

        self._reg_write_by_name_concat(model, fefilt_selected, reg_name_str, value)
