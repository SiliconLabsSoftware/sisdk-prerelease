from pyradioconfig.parts.ocelot.calculators.calc_frame import CALC_Frame_ocelot

class Calc_Frame_Sol(CALC_Frame_ocelot):

    def _configure_variable_length(self, model):
        demod_select = model.vars.demod_select.value

        super(Calc_Frame_Sol, self)._configure_variable_length(model)

        if demod_select == model.vars.demod_select.var_enum.SOFT_DEMOD:
            self._reg_write(model.vars.FRC_DFLCTRL_DFLMODE, 0)

    def _fixed_length_no_header(self, model):
        """_fixed_length_no_header

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        super()._fixed_length_no_header(model)

        softmodem_modulation_type = model.vars.softmodem_modulation_type.value
        if softmodem_modulation_type == model.vars.softmodem_modulation_type.var_enum.SUN_FSK:
            fcdDict = {
                "excludesubframewcnt": int(model.vars.payload_excludesubframewcnt_en.value == True),
                "addtrailtxdata": int(model.vars.payload_addtrailtxdata_en.value == True),
                "skipwhite": int(model.vars.payload_white_en.value == True),
                "skipcrc": 0,
                "calccrc": int(model.vars.payload_crc_en.value == False),
                "includecrc": int(model.vars.payload_crc_en.value == False),
                "words": 0xFF,
            }
            self._configure_fcd(model, fcdindex="0", buf=0, **fcdDict)

        return

    def calc_frame_length(self, model):
        """calc_frame_length

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        softmodem_modulation_type = model.vars.softmodem_modulation_type.value
        if softmodem_modulation_type == model.vars.softmodem_modulation_type.var_enum.SUN_FSK:
            self._calc_frame_length_defaults(model)
            self._fixed_length_no_header(model)
        else:
            super().calc_frame_length(model)

        return

    def calc_softmodem_frame_dont_cares(self, model):
        softmodem_modulation_type = model.vars.softmodem_modulation_type.value
        dual_fefilt = model.vars.dual_fefilt.value

        if (not dual_fefilt) and (softmodem_modulation_type != model.vars.softmodem_modulation_type.var_enum.NONE):
                for model_var in model.vars:
                    if ("FRC_FCD" in model_var.name):
                            model_var.value_do_not_care = True