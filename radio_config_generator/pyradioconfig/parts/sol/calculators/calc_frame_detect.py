from pyradioconfig.parts.ocelot.calculators.calc_frame_detect import CALC_Frame_Detect_Ocelot

class Calc_Frame_Detect_Sol(CALC_Frame_Detect_Ocelot):

    def calc_syncerrors_reg(self, model):

        demod_select = model.vars.demod_select.value
        rtschmode_actual = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value
        mod_type = model.vars.modulation_type.value

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
                demod_select == model.vars.demod_select.var_enum.TRECS_SLICER or \
                (demod_select == model.vars.demod_select.var_enum.SOFT_DEMOD and \
                 mod_type == model.vars.modulation_type.var_enum.FSK2):
            if rtschmode_actual == 1:
                syncerrors = 1
            else:
                syncerrors = 0
        else:
            syncerrors = 0

        self._reg_write(model.vars.MODEM_CTRL1_SYNCERRORS, syncerrors)

    def calc_syncword_tx_skip(self, model):

        syncword_tx_skip = model.vars.syncword_tx_skip.value
        softmodem_active = (model.vars.softmodem_modulation_type.value != model.vars.softmodem_modulation_type.var_enum.NONE)

        if syncword_tx_skip or softmodem_active:
            syncdata = 1
        else:
            syncdata = 0

        self._reg_write(model.vars.MODEM_CTRL1_SYNCDATA, syncdata)


