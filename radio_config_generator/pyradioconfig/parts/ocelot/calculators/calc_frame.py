from pyradioconfig.parts.panther.calculators.calc_frame import CALC_Frame_panther

class CALC_Frame_ocelot(CALC_Frame_panther):

    def calc_possible_future_inputs(self, model):

        model.vars.header_include_crc.value = False
        model.vars.header_addtrailtxdata_en.value = False
        model.vars.payload_addtrailtxdata_en.value = False

        return

    def calc_excludesubframewcnt_en(self, model):

        ber_force_infinite_length = model.vars.ber_force_infinite_length.value

        if ber_force_infinite_length:
            header_excludesubframewcnt_en = True
            payload_excludesubframewcnt_en = True
        else:
            header_excludesubframewcnt_en = False
            payload_excludesubframewcnt_en = False

        model.vars.header_excludesubframewcnt_en.value = header_excludesubframewcnt_en
        model.vars.payload_excludesubframewcnt_en.value = payload_excludesubframewcnt_en

    def calc_ctrl2_rxfrcdis(self, model):

        directmode_rx = model.vars.directmode_rx.value

        if directmode_rx != model.vars.directmode_rx.var_enum.DISABLED:
            rxfrcdis = 1
        else:
            rxfrcdis = 0

        self._reg_write(model.vars.MODEM_CTRL2_RXFRCDIS, rxfrcdis)