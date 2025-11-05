from pyradioconfig.parts.leopard.calculators.calc_frame import calc_frame_leopard


class CalcFrameLion(calc_frame_leopard):

    def calc_ctrl_lpmodedis_reg(self, model):
        # This method calculates the LPMODEDIS field
        # https://jira.silabs.com/browse/MCUW_RADIO_CFG-2473

        # Save 40uA current for all phys during RXSEARCH by enabling LPMODE
        lpmodedis = 0

        # Write the register
        self._reg_write(model.vars.FRC_CTRL_LPMODEDIS, lpmodedis)