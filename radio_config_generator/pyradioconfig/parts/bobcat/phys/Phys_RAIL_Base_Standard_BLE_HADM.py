from pyradioconfig.calculator_model_framework.decorators.phy_decorators import do_not_inherit_phys
from pyradioconfig.parts.bobcat.phys.Phys_RAIL_Base_Standard_BLE import PHYS_Bluetooth_LE_Bobcat
from py_2_and_3_compatibility import *


@do_not_inherit_phys
class PHYS_Bluetooth_LE_Bobcat_HADM(PHYS_Bluetooth_LE_Bobcat):

    default_xtal_frequency_hz = 40000000
    
    def PHY_Bluetooth_1M_HADM(self, model, phy_name=None):
        phy = super().PHY_Bluetooth_LE_Viterbi_noDSA_fullrate(model, phy_name=phy_name)
        model.vars.ble_feature.value_forced = model.vars.ble_feature.var_enum.HADM_1M
        self._set_xtal_frequency(phy=phy, xtal_freq_hz=40000000)
        model.vars.adc_clock_mode.value_forced = model.vars.adc_clock_mode.var_enum.HFXOMULT

        phy.profile_inputs.target_osr.value = 5

        phy.profile_inputs.frame_bitendian.value = model.vars.frame_bitendian.var_enum.LSB_FIRST
        phy.profile_inputs.frame_length_type.value = model.vars.frame_length_type.var_enum.FIXED_LENGTH
        phy.profile_inputs.payload_white_en.value = False
        phy.profile_inputs.payload_crc_en.value = False

        phy.profile_inputs.header_en.value = False

        phy.profile_outputs.FRC_FCD0_INCLUDECRC.override = 0
        phy.profile_outputs.FRC_FCD2_INCLUDECRC.override = 0

        phy.profile_outputs.MODEM_REALTIMCFE_VTAFCFRAME.override = 1  # estimate from syncword
        phy.profile_outputs.MODEM_AFC_AFCONESHOT.override = 0
        phy.profile_outputs.MODEM_AFC_AFCRXMODE.override = 0  # no estimate from DSA
        phy.profile_outputs.MODEM_AFCADJRX_AFCSCALEM.override = 0  # disable feedback to avoid updates during tracking cycle
        phy.profile_outputs.MODEM_AFCADJRX_AFCSCALEE.override = 0

        phy.profile_outputs.rx_sync_delay_ns.override = 24000
        phy.profile_outputs.rx_eof_delay_ns.override = 8000
        phy.profile_outputs.tx_sync_delay_ns.override = 0
        phy.profile_outputs.tx_eof_delay_ns.override = 0

        return phy

    def PHY_Bluetooth_2M_HADM(self, model, phy_name=None):
        phy = super().PHY_Bluetooth_LE_2M_Viterbi_noDSA_fullrate(model, phy_name=phy_name)
        model.vars.ble_feature.value_forced = model.vars.ble_feature.var_enum.HADM_2M
        self._set_xtal_frequency(phy=phy, xtal_freq_hz=40000000)
        model.vars.adc_clock_mode.value_forced = model.vars.adc_clock_mode.var_enum.HFXOMULT
        phy.profile_inputs.target_osr.value = 5

        phy.profile_inputs.frame_bitendian.value = model.vars.frame_bitendian.var_enum.LSB_FIRST
        phy.profile_inputs.frame_length_type.value = model.vars.frame_length_type.var_enum.FIXED_LENGTH
        phy.profile_inputs.payload_white_en.value = False
        phy.profile_inputs.payload_crc_en.value = False

        phy.profile_inputs.header_en.value = False

        phy.profile_outputs.FRC_FCD0_INCLUDECRC.override = 0
        phy.profile_outputs.FRC_FCD2_INCLUDECRC.override = 0

        phy.profile_outputs.MODEM_REALTIMCFE_VTAFCFRAME.override = 1 # estimate from syncword
        phy.profile_outputs.MODEM_AFC_AFCONESHOT.override = 0
        phy.profile_outputs.MODEM_AFC_AFCRXMODE.override = 0 # no estimate from DSA
        phy.profile_outputs.MODEM_AFCADJRX_AFCSCALEM.override = 0 # disable feedback to avoid updates during tracking cycle
        phy.profile_outputs.MODEM_AFCADJRX_AFCSCALEE.override = 0

        return phy
