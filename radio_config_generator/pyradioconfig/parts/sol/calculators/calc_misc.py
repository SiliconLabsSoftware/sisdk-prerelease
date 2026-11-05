from pyradioconfig.parts.ocelot.calculators.calc_misc import CALC_Misc_ocelot
from pycalcmodel.core.variable import ModelVariableFormat

class Calc_Misc_Sol(CALC_Misc_ocelot):

    def buildVariables(self, model):
        super().buildVariables(model)

        self._addModelVariable(model, 'alt_stack_info', int, ModelVariableFormat.DECIMAL,
                               desc='information dedicated to stack for alternate PHY', is_array=True)

    def calc_misc_Ocelot(self, model):
        pass

    def calc_misc_seq_dig_ramp_en_reg(self, model):
        pass

    def calc_misc_Sol(self, model):
        """
        These aren't really calculating right now.  Just using defaults or forced values.
       fefilt_selected = model.vars.fefilt_selected.value
        Args:
             model (ModelRoot) : Data model to read and write variables from
        """
        fefilt_selected = model.vars.fefilt_selected.value

        
        self._reg_write_by_name_concat(model, fefilt_selected, 'CF_ADCBITORDERI', 0)
        self._reg_write_by_name_concat(model, fefilt_selected, 'CF_ADCBITORDERQ', 0)
        
        
        self._reg_write_by_name_concat(model, fefilt_selected, 'DIGIGAINCTRL_DEC0GAIN', 0)
        self._reg_write_by_name_concat(model, fefilt_selected, 'DIGIGAINCTRL_DIGIGAINEN', 0)
        self._reg_write_by_name_concat(model, fefilt_selected, 'DIGIGAINCTRL_DIGIGAIN', 0)

        self._reg_write_by_name_concat(model, fefilt_selected, 'CHFCTRL_FWSELCOEFF', 0)
        self._reg_write_by_name_concat(model, fefilt_selected, 'CHFCTRL_FWSWCOEFFEN', 0)
        self._reg_write_by_name_concat(model, fefilt_selected, 'CHFCTRL_CHFLATENCY', 0)

        self._reg_write_by_name_concat(model, fefilt_selected, 'DCCOMPFILTINIT_DCCOMPINIT', 0)
        self._reg_write_by_name_concat(model, fefilt_selected, 'DCCOMPFILTINIT_DCCOMPINITVALI', 0)
        self._reg_write_by_name_concat(model, fefilt_selected, 'DCCOMPFILTINIT_DCCOMPINITVALQ', 0)

        self._reg_write(model.vars.MODEM_PHDMODANTDIV_ANTDECRSTBYP, 1)
        self._reg_write(model.vars.MODEM_PHDMODANTDIV_RECHKCORREN,  0)
        self._reg_write(model.vars.MODEM_PHDMODANTDIV_SKIPTHDSEL,   1)

        self._reg_write(model.vars.RAC_PATRIM6_TXTRIMBBREGFB, 8)

    def calc_adc_sidetone_amp_reg(self, model):
        self._reg_write(model.vars.RAC_IFADCTRIM0_IFADCSIDETONEAMP, 3)

    def calc_phy_mode_id(self, model):
        protocol_id = model.vars.protocol_id.value
        softmodem_modulation_type = model.vars.softmodem_modulation_type.value
        header_size = model.vars.header_size.value
        fec_tx_enable = model.vars.fec_tx_enable.value

        specific_parameters = []

        if softmodem_modulation_type == model.vars.softmodem_modulation_type.var_enum.SUN_OQPSK:
            model.vars.phy_mode_id.value = int(model.vars.wisun_phy_mode_id.value[0])
        elif protocol_id == model.vars.protocol_id.var_enum.Connect and \
            softmodem_modulation_type == model.vars.softmodem_modulation_type.var_enum.SUN_OFDM:
            model.vars.phy_mode_id.value = model.vars.wisun_phy_mode_id.value[0]
        elif protocol_id == model.vars.protocol_id.var_enum.Connect and header_size == 2: # : Connect SUN-FSK is two byte header
            if fec_tx_enable == model.vars.fec_tx_enable.var_enum.ENABLED:
                model.vars.phy_mode_id.value = model.vars.wisun_phy_mode_id.value[1]
            else:
                model.vars.phy_mode_id.value = model.vars.wisun_phy_mode_id.value[0]
        else:
            super().calc_phy_mode_id(model)

    def calc_stack_info(self, model):
        protocol_id = model.vars.protocol_id.value
        softmodem_modulation_type = model.vars.softmodem_modulation_type.value
        wisun_channel_plan_id = getattr(model.profile.inputs, "wisun_channel_plan_id", None)
        wisun_reg_domain = getattr(model.profile.inputs, "wisun_reg_domain", None)
        header_size = model.vars.header_size.value
        phy_id = model.vars.phy_mode_id.value

        if softmodem_modulation_type == model.vars.softmodem_modulation_type.var_enum.SUN_OQPSK:
            version = 1  # FAN1.1 version
            if wisun_channel_plan_id is None:
                wisun_channel_plan_id = 255
            if wisun_reg_domain is None:
                wisun_reg_domain = 255
            model.vars.stack_info.value = [int(protocol_id), int(phy_id), version, wisun_channel_plan_id, wisun_reg_domain]
        elif (protocol_id == model.vars.protocol_id.var_enum.Connect and softmodem_modulation_type == model.vars.softmodem_modulation_type.var_enum.SUN_OFDM) \
                or (protocol_id == model.vars.protocol_id.var_enum.Connect and header_size == 2):
            model.vars.stack_info.value = [int(protocol_id), int(phy_id)]
        else:
            super().calc_stack_info(model)