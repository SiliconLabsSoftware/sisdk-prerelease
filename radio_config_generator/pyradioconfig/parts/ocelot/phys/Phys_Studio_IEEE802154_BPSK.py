from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pyradioconfig.parts.common.phys.phy_common import PHY_COMMON_FRAME_154
from py_2_and_3_compatibility import *


class PhysStudioIEEE802154BPSKOcelot(IPhy):

    def PHY_IEEE802154_868MHz_BPSK_20kbps_prod(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.IEEE802154BPSK,
                            readable_name='Production IEEE802154 BPSK 868MHz 20kbps PHY',
                            phy_name=phy_name)
        phy.profile_inputs.base_frequency_hz.value = 868_000_000
        phy.profile_inputs.bitrate.value = 20_000
        phy.profile_inputs.bpsk_feature.value = model.vars.bpsk_feature.var_enum.STANDARD_20KBPS

        phy.profile_inputs.chcfg_base_frequency_hz.value = 868_300_000
        phy.profile_inputs.chcfg_channel_spacing_hz.value = 200_000

        phy.profile_inputs.chcfg_channel_number_start.value = 0
        phy.profile_inputs.chcfg_channel_number_end.value = 0
        phy.profile_inputs.chcfg_physical_channel_offset.value = 0

        phy.profile_inputs.rail_tx_power_max.value = [-1]

        self._set_xtal_frequency(phy)

        return phy

    def PHY_IEEE802154_915MHz_BPSK_40kbps_prod(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.IEEE802154BPSK,
                            readable_name='Production IEEE802154 BPSK 915MHz 40kbps PHY',
                            phy_name=phy_name)
        phy.profile_inputs.base_frequency_hz.value = 915_000_000
        phy.profile_inputs.bitrate.value = 40_000
        phy.profile_inputs.bpsk_feature.value = model.vars.bpsk_feature.var_enum.STANDARD_40KBPS

        phy.profile_inputs.chcfg_base_frequency_hz.value = 915_000_000
        phy.profile_inputs.chcfg_channel_spacing_hz.value = 200_000

        phy.profile_inputs.chcfg_channel_number_start.value = 0
        phy.profile_inputs.chcfg_channel_number_end.value = 0
        phy.profile_inputs.chcfg_physical_channel_offset.value = 0

        phy.profile_inputs.rail_tx_power_max.value = [-1]

        self._set_xtal_frequency(phy)

        return phy

    def _set_xtal_frequency(self, phy, xtal_freq=None):
        if xtal_freq is None:
            phy.profile_inputs.xtal_frequency_hz.value = 39000000
        else:
            phy.profile_inputs.xtal_frequency_hz.value = xtal_freq

    def IEEE802154_Base(self, phy, model):
        # Inputs
        phy.profile_inputs.diff_encoding_mode.value = model.vars.diff_encoding_mode.var_enum.DISABLED
        phy.profile_inputs.fsk_symbol_map.value = model.vars.fsk_symbol_map.var_enum.MAP0
        phy.profile_inputs.preamble_pattern.value = 0
        phy.profile_inputs.preamble_pattern_len.value = 4
        phy.profile_inputs.rx_xtal_error_ppm.value = 0
        phy.profile_inputs.symbol_encoding.value = model.vars.symbol_encoding.var_enum.DSSS
        phy.profile_inputs.syncword_0.value = long(0xe5)
        phy.profile_inputs.syncword_1.value = long(0x0)
        phy.profile_inputs.syncword_length.value = 8
        phy.profile_inputs.tx_xtal_error_ppm.value = 0
        self._set_xtal_frequency(phy)

        # Add 15.4 Packet Configuration
        PHY_COMMON_FRAME_154(phy, model)
        
