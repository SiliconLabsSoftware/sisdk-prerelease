from pyradioconfig.parts.rainier.phys.Phys_RAIL_Base_Standard_BLE import PhysRailBaseStandardBleRainier
from pyradioconfig.calculator_model_framework.decorators.phy_decorators import do_not_inherit_phys
from pyradioconfig.parts.bobcat.phys.Phys_RAIL_Base_Standard_BLE_HADM import PHYS_Bluetooth_LE_Bobcat_HADM
from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy

@do_not_inherit_phys
class PhysRailBaseStandardBleHadmRainier(PHYS_Bluetooth_LE_Bobcat_HADM, PhysRailBaseStandardBleRainier):

    default_xtal_frequency_hz = 40000000

    def override_other_shaping_coeff_to_zero(self, phy, start_coeff = 0):
        total_coeff = 64 # for Viper we have maximum of 64 filter tap coefficients
        for coeff in range(start_coeff, total_coeff):
            register_group = int(coeff / 4)
            command = "phy.profile_outputs.MODEM_SHAPING"+str(register_group)+"_COEFF"+str(coeff)+".override = 0"
            exec(command)


    def _init_hadm_rdm_state(self, model, phy):
        phy.profile_inputs.tx_rdm_state.value = model.vars.tx_rdm_state.var_enum.TX_HADM
        phy.profile_inputs.rx_rdm_state.value = model.vars.rx_rdm_state.var_enum.RX_HADM_RFPKD



    def PHY_Bluetooth_1M_HADM(self, model, phy_name='PHY_Bluetooth_1M_HADM'):
        phy = super().PHY_Bluetooth_1M_HADM(model)
        phy.profile_inputs.modulator_select.value = model.vars.modulator_select.var_enum.IQ_MOD
        phy.profile_inputs.synchronous_ifadc_clk.value = True
        phy.profile_inputs.synchronous_mixdac_clk.value = True
        self.BLE_TX_Shaping_Coeffs_0p5bt_IQMOD(phy)
        model.vars.synth_tx_mode.value_forced = model.vars.synth_tx_mode.var_enum.MODE_IQMOD
        self._init_hadm_rdm_state(model, phy)

        return phy

    def PHY_Bluetooth_2M_HADM(self, model, phy_name='PHY_Bluetooth_2M_HADM'):
        phy = super().PHY_Bluetooth_2M_HADM(model)
        phy.profile_inputs.modulator_select.value = model.vars.modulator_select.var_enum.IQ_MOD
        phy.profile_inputs.synchronous_ifadc_clk.value = True
        phy.profile_inputs.synchronous_mixdac_clk.value = True
        self.BLE_TX_Shaping_Coeffs_0p5bt_IQMOD(phy)
        model.vars.synth_tx_mode.value_forced = model.vars.synth_tx_mode.var_enum.MODE_IQMOD
        self._init_hadm_rdm_state(model, phy)

        return phy

    def PHY_Bluetooth_2M_HADM_2BT(self, model, phy_name='PHY_Bluetooth_2M_HADM_2bt'):
        phy = self.PHY_Bluetooth_2M_HADM(model)
        phy.profile_inputs.shaping_filter_param.value = 2.0
        self.BLE_TX_Shaping_Coeffs_2bt_IQMOD(phy)
        model.vars.ble_feature.value_forced = model.vars.ble_feature.var_enum.HADM_2M_2BT
        model.vars.synth_tx_mode.value_forced = model.vars.synth_tx_mode.var_enum.MODE_IQMOD
        self._init_hadm_rdm_state(model, phy)

        return phy



    def PHY_Bluetooth_1M_HADM_prod(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.BLE, readable_name='Production BLE HADM 1Mbps PHY',
                            phy_name=phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=40000000)
        phy.profile_inputs.ble_feature.value = model.vars.ble_feature.var_enum.HADM_1M

        phy.profile_inputs.chcfg_channel_number_start.value = 0
        phy.profile_inputs.chcfg_channel_number_end.value = 39
        phy.profile_inputs.chcfg_physical_channel_offset.value = 0

        phy.profile_inputs.rail_tx_power_max.value = [-1] * 40
        return phy

    def PHY_Bluetooth_2M_HADM_prod(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.BLE, readable_name='Production BLE HADM 2Mbps PHY',
                            phy_name=phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=40000000)
        phy.profile_inputs.ble_feature.value = model.vars.ble_feature.var_enum.HADM_2M

        phy.profile_inputs.chcfg_channel_number_start.value = 0
        phy.profile_inputs.chcfg_channel_number_end.value = 39
        phy.profile_inputs.chcfg_physical_channel_offset.value = 0

        phy.profile_inputs.rail_tx_power_max.value = [-1] * 40
        return phy

    def PHY_Bluetooth_2M_HADM_2BT_prod(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.BLE, readable_name='Production BLE HADM 2Mbps 2BT PHY',
                            phy_name=phy_name)
        self._set_xtal_frequency(phy, xtal_freq_hz=40000000)
        phy.profile_inputs.ble_feature.value = model.vars.ble_feature.var_enum.HADM_2M_2BT

        phy.profile_inputs.chcfg_channel_number_start.value = 0
        phy.profile_inputs.chcfg_channel_number_end.value = 39
        phy.profile_inputs.chcfg_physical_channel_offset.value = 0

        phy.profile_inputs.rail_tx_power_max.value = [-1] * 40
        return phy



    def BLE_TX_Shaping_Coeffs_0p5bt_IQMOD(self, phy):
        phy.profile_outputs.MODEM_CTRL0_SHAPING.override = 2

        phy.profile_outputs.MODEM_SHAPING0_COEFF0.override = 6
        phy.profile_outputs.MODEM_SHAPING0_COEFF1.override = 15
        phy.profile_outputs.MODEM_SHAPING0_COEFF2.override = 30
        phy.profile_outputs.MODEM_SHAPING0_COEFF3.override = 52
        phy.profile_outputs.MODEM_SHAPING1_COEFF4.override = 76
        phy.profile_outputs.MODEM_SHAPING1_COEFF5.override = 97
        phy.profile_outputs.MODEM_SHAPING1_COEFF6.override = 112
        phy.profile_outputs.MODEM_SHAPING1_COEFF7.override = 120

        self.override_other_shaping_coeff_to_zero(phy, start_coeff=8)

    def BLE_TX_Shaping_Coeffs_2bt_IQMOD(self, phy):
        phy.profile_outputs.MODEM_CTRL0_SHAPING.override = 2

        phy.profile_outputs.MODEM_SHAPING0_COEFF0.override = 0
        phy.profile_outputs.MODEM_SHAPING0_COEFF1.override = 0
        phy.profile_outputs.MODEM_SHAPING0_COEFF2.override = 0
        phy.profile_outputs.MODEM_SHAPING0_COEFF3.override = 16
        phy.profile_outputs.MODEM_SHAPING1_COEFF4.override = 111
        phy.profile_outputs.MODEM_SHAPING1_COEFF5.override = 127
        phy.profile_outputs.MODEM_SHAPING1_COEFF6.override = 127
        phy.profile_outputs.MODEM_SHAPING1_COEFF7.override = 127

        self.override_other_shaping_coeff_to_zero(phy, start_coeff=8)

