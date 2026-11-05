from pyradioconfig.parts.common.calculators.calc_white import CALC_Whitening

class CALC_Whitening_Ocelot(CALC_Whitening):

    def _set_block_coder_white_settings(self, model):

        symbol_encoding = model.vars.symbol_encoding.value
        demod_select = model.vars.demod_select.value

        if (symbol_encoding == model.vars.symbol_encoding.var_enum.Manchester or
            symbol_encoding == model.vars.symbol_encoding.var_enum.Inv_Manchester) and \
                demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI:
            self._reg_write(model.vars.FRC_WHITECTRL_XORFEEDBACK, 0)
            self._reg_write(model.vars.FRC_WHITECTRL_FEEDBACKSEL, 16)
            self._reg_write(model.vars.FRC_WHITEINIT_WHITEINIT, 0)

            self._reg_write(model.vars.FRC_WHITECTRL_SHROUTPUTSEL, model.vars.frame_coding_fshroutputsel_val.value)
            self._reg_write(model.vars.FRC_WHITEPOLY_POLY, model.vars.frame_coding_poly_val.value)
        else:
            super().calc_white_settings(model)

    def calc_white_settings(self, model):
        self._set_block_coder_white_settings(model)

        self._reg_write(model.vars.FRC_CTRL_SKIPTXTRAILDATAWHITEN, 0x0)
        self._reg_write(model.vars.FRC_CTRL_SKIPRXSUPSTATEWHITEN, 0x0)
