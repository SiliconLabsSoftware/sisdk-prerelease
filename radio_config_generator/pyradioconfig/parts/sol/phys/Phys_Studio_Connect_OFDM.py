from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pycalcmodel.core.common import IPCategory


class Phys_Studio_Connect_OFDM_Sol(IPhy):

    ip_category = IPCategory.LPW

    def PHY_Studio_Connect_OFDM_915MHz_BW_1p2MHz_NA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Connect_OFDM, readable_name='Connect OFDM NA-915MHz, 1.2 MHz BW', phy_name=phy_name)

        phy.profile_inputs.ofdm_option.value = model.vars.ofdm_option.var_enum.OPT1_OFDM_BW_1p2MHz

        phy.profile_inputs.base_frequency_hz.value = 903200000
        phy.profile_inputs.channel_spacing_hz.value = 1200000

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)

    def PHY_Studio_Connect_OFDM_915MHz_BW_0p8MHz_NA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Connect_OFDM, readable_name='Connect OFDM NA-915MHz, 0.8 MHz BW', phy_name=phy_name)

        phy.profile_inputs.ofdm_option.value = model.vars.ofdm_option.var_enum.OPT2_OFDM_BW_0p8MHz

        phy.profile_inputs.base_frequency_hz.value = 902800000
        phy.profile_inputs.channel_spacing_hz.value = 800000

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)

    def PHY_Studio_Connect_OFDM_915MHz_BW_0p4MHz_NA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Connect_OFDM, readable_name='Connect OFDM NA-915MHz, 0.4 MHz BW', phy_name=phy_name)

        phy.profile_inputs.ofdm_option.value = model.vars.ofdm_option.var_enum.OPT3_OFDM_BW_0p4MHz

        phy.profile_inputs.base_frequency_hz.value = 902400000
        phy.profile_inputs.channel_spacing_hz.value = 400000

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)

    def PHY_Studio_Connect_OFDM_915MHz_BW_0p2MHz_NA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Connect_OFDM, readable_name='Connect OFDM NA-915MHz, 0.2 MHz BW', phy_name=phy_name)

        phy.profile_inputs.ofdm_option.value = model.vars.ofdm_option.var_enum.OPT4_OFDM_BW_0p2MHz

        phy.profile_inputs.base_frequency_hz.value = 902200000
        phy.profile_inputs.channel_spacing_hz.value = 200000

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)
