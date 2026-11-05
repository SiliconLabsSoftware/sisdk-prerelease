from pyradioconfig.parts.curl.phys.Phys_RAIL_Base_Standard_BLE import PhysRailBaseStandardBluetoothLeCurl
from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pycalcmodel.core.common import IPCategory



class PhysRailBaseStandardBluetoothHadmCurl(IPhy):

    ip_category = IPCategory.LPW
    default_xtal_frequency_hz = 40000000

    def PHY_Bluetooth_1M_HADM(self, model, phy_name='PHY_Bluetooth_1M_HADM'):
        phy = PhysRailBaseStandardBluetoothLeCurl().PHY_Bluetooth_LE_Viterbi_noDSA_fullrate(model, phy_name=phy_name)
        model.vars.ble_feature.value_forced = model.vars.ble_feature.var_enum.HADM_1M
        self._set_xtal_frequency(phy, xtal_freq_hz=40000000)
        phy.profile_inputs.bandwidth_hz.value = 1100000
        phy.profile_inputs.hadm_enable.value = model.vars.hadm_enable.var_enum.ENABLED
        model.vars.adc_clock_mode.value_forced = model.vars.adc_clock_mode.var_enum.HFXOMULT

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
        phy.profile_outputs.MODEM_AFC_AFCSCALEM.override = 0  # disable feedback to avoid updates during tracking cycle
        phy.profile_outputs.MODEM_AFC_AFCSCALEE.override = 0

        phy.profile_outputs.rx_sync_delay_ns.override = 24000
        phy.profile_outputs.rx_eof_delay_ns.override = 8000
        phy.profile_outputs.tx_sync_delay_ns.override = 0
        phy.profile_outputs.tx_eof_delay_ns.override = 0

        phy.profile_outputs.AGC_CTRL0_MODE.override = 3

        return phy

    def PHY_Bluetooth_2M_HADM(self, model, phy_name='PHY_Bluetooth_2M_HADM'):
        phy = PhysRailBaseStandardBluetoothLeCurl().PHY_Bluetooth_LE_2M_Viterbi_noDSA_fullrate(model, phy_name=phy_name)
        model.vars.ble_feature.value_forced = model.vars.ble_feature.var_enum.HADM_2M
        self._set_xtal_frequency(phy, xtal_freq_hz=40000000)
        phy.profile_inputs.hadm_enable.value = model.vars.hadm_enable.var_enum.ENABLED
        model.vars.adc_clock_mode.value_forced = model.vars.adc_clock_mode.var_enum.HFXOMULT

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
        phy.profile_outputs.MODEM_AFC_AFCSCALEM.override = 0  # disable feedback to avoid updates during tracking cycle
        phy.profile_outputs.MODEM_AFC_AFCSCALEE.override = 0

        phy.profile_outputs.AGC_CTRL0_MODE.override = 3

        return phy

    def PHY_Bluetooth_2M_HADM_2BT(self, model, phy_name='PHY_Bluetooth_2M_HADM_2BT'):
        phy = self.PHY_Bluetooth_2M_HADM(model, phy_name)
        model.vars.ble_feature.value_forced = model.vars.ble_feature.var_enum.HADM_2M_2BT

        phy.profile_inputs.shaping_filter_param.value = 2.0
        phy.profile_outputs.MODEM_VITERBIDEMOD_VITERBIKSI1.override = 64
        phy.profile_outputs.MODEM_VITERBIDEMOD_VITERBIKSI2.override = 53
        phy.profile_outputs.MODEM_VITERBIDEMOD_VITERBIKSI3.override = 41

        return phy



    def PHY_Bluetooth_1M_HADM_38MHz(self, model, phy_name='PHY_Bluetooth_1M_HADM_38MHz'):
        phy = self.PHY_Bluetooth_1M_HADM(model, phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=38000000)

        return phy

    def PHY_Bluetooth_1M_HADM_38p4MHz(self, model, phy_name='PHY_Bluetooth_1M_HADM_38p4MHz'):
        phy = self.PHY_Bluetooth_1M_HADM(model, phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=38400000)

        return phy

    def PHY_Bluetooth_1M_HADM_39MHz(self, model, phy_name='PHY_Bluetooth_1M_HADM_39MHz'):
        phy = self.PHY_Bluetooth_1M_HADM(model, phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)

        return phy

    def PHY_Bluetooth_1M_HADM_40MHz(self, model, phy_name='PHY_Bluetooth_1M_HADM_40MHz'):
        phy = self.PHY_Bluetooth_1M_HADM(model, phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=40000000)

        return phy

    def PHY_Bluetooth_2M_HADM_38MHz(self, model, phy_name='PHY_Bluetooth_2M_HADM_38MHz'):
        phy = self.PHY_Bluetooth_2M_HADM(model, phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=38000000)

        return phy

    def PHY_Bluetooth_2M_HADM_38p4MHz(self, model, phy_name='PHY_Bluetooth_2M_HADM_38p4MHz'):
        phy = self.PHY_Bluetooth_2M_HADM(model, phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=38400000)

        return phy

    def PHY_Bluetooth_2M_HADM_39MHz(self, model, phy_name='PHY_Bluetooth_2M_HADM_39MHz'):
        phy = self.PHY_Bluetooth_2M_HADM(model, phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)

        return phy

    def PHY_Bluetooth_2M_HADM_40MHz(self, model, phy_name='PHY_Bluetooth_2M_HADM_40MHz'):
        phy = self.PHY_Bluetooth_2M_HADM(model, phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=40000000)

        return phy

    def PHY_Bluetooth_2M_HADM_2BT_38MHz(self, model, phy_name='PHY_Bluetooth_2M_HADM_2BT_38MHz'):
        phy = self.PHY_Bluetooth_2M_HADM_2BT(model, phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=38000000)

        return phy

    def PHY_Bluetooth_2M_HADM_2BT_38p4MHz(self, model, phy_name='PHY_Bluetooth_2M_HADM_2BT_38p4MHz'):
        phy = self.PHY_Bluetooth_2M_HADM_2BT(model, phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=38400000)

        return phy

    def PHY_Bluetooth_2M_HADM_2BT_39MHz(self, model, phy_name='PHY_Bluetooth_2M_HADM_2BT_39MHz'):
        phy = self.PHY_Bluetooth_2M_HADM_2BT(model, phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)

        return phy

    def PHY_Bluetooth_2M_HADM_2BT_40MHz(self, model, phy_name='PHY_Bluetooth_2M_HADM_2BT_40MHz'):
        phy = self.PHY_Bluetooth_2M_HADM_2BT(model, phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=40000000)

        return phy

