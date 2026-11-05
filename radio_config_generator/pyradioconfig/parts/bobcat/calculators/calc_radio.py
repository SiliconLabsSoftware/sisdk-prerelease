from pyradioconfig.parts.ocelot.calculators.calc_radio import CALC_Radio_ocelot

class Calc_Radio_Bobcat(CALC_Radio_ocelot):

    def calc_lnamix_reg(self, model):
        self._reg_write(model.vars.RAC_LNAMIXTRIM4_LNAMIXRFPKDTHRESHSELLO, 1)
        self._reg_write(model.vars.RAC_LNAMIXTRIM4_LNAMIXRFPKDTHRESHSELHI, 3)
        self._reg_write(model.vars.RAC_PGACTRL_PGATHRPKDLOSEL, 1)
        self._reg_write(model.vars.RAC_PGACTRL_PGATHRPKDHISEL, 5)

    def calc_txtrimdregbleed_reg(self, model):
        pass