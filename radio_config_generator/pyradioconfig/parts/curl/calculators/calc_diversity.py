from pyradioconfig.calculator_model_framework.Utils.CustomExceptions import CalculationException
from pyradioconfig.parts.common.calculators.calc_diversity import CALC_Diversity


class CalcDiversityCurl(CALC_Diversity):

    def calc_diversity_values(self, model):
        model.vars.antdivmode.value = model.vars.antdivmode.var_enum.DISABLE
        model.vars.antdivrepeatdis.value = model.vars.antdivrepeatdis.var_enum.REPEATFIRST

        antdivmode = model.vars.antdivmode.value
        antdivrepeatdis = model.vars.antdivrepeatdis.value

        flag_using_viterbi_demod = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value == 1

        if not flag_using_viterbi_demod:
            model.vars.div_antdivmode.value = int(antdivmode)
            model.vars.div_antdivrepeatdis.value = int(antdivrepeatdis)
        else:

            model.vars.div_antdivmode.value = int(model.vars.antdivmode.var_enum.DISABLE)
            model.vars.div_antdivrepeatdis.value = int(model.vars.antdivrepeatdis.var_enum.REPEATFIRST)

            if antdivmode != int(model.vars.antdivmode.var_enum.DISABLE) and flag_using_Viterbi_demod:
                raise CalculationException("Cannot enable antenna diversity when using Viterbi demodulation.")
