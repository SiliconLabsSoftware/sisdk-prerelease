from pyradioconfig.parts.bobcat.calculators.calc_radio import Calc_Radio_Bobcat


class CalcRadioRainier(Calc_Radio_Bobcat):

    def calc_lpfbwtx_reg(self, model):
        pass

    def calc_lnamix_reg(self, model):
        self._reg_write(model.vars.RAC_LNAMIXCTRL1_LNAMIXRFPKDTHRESHSELHI, 5)
        self._reg_write(model.vars.RAC_LNAMIXCTRL1_LNAMIXRFPKDTHRESHSELLO, 2)

        self._reg_write(model.vars.RAC_TIACTRL0_TIATHRPKDHISEL, 6)
        self._reg_write(model.vars.RAC_TIACTRL0_TIATHRPKDLOSEL, 2)

    def calc_mxrlosel_reg(self, model):
        rx_rdm_state = model.vars.rx_rdm_state.value

        if rx_rdm_state in [model.vars.rx_rdm_state.var_enum.RX_HADM_RFPKD, model.vars.rx_rdm_state.var_enum.RX_HADM]:
            self._reg_write(model.vars.RAC_LNAMIXEN1_LNAMIXMXRLOSEL, 1)
        else:
            self._reg_write(model.vars.RAC_LNAMIXEN1_LNAMIXMXRLOSEL,0)
