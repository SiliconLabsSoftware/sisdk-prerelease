from pyradioconfig.parts.common.calculators.calc_ber import CALC_Ber

class CALC_Ber_Ocelot(CALC_Ber):

    def calc_test_ber(self,model):

        model.vars.test_ber.value = False

    def calc_per_forces(self,model):
        pass