from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pycalcmodel.core.common import IPCategory

class Phys_Studio_SUN_OQPSK_Sol(IPhy):

    ip_category = IPCategory.LPW

    def PHY_IEEE802154_SUN_920MHz_OQPSK_100kcps_JP(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.SUN_OQPSK, readable_name='SUN JP-920MHz, OQPSK 100kcps',
                            phy_name=phy_name)

        phy.profile_inputs.sun_oqpsk_chiprate.value = model.vars.sun_oqpsk_chiprate.var_enum._100_KCPS

        phy.profile_inputs.base_frequency_hz.value = 920600000
        phy.profile_inputs.channel_spacing_hz.value = 200000
        phy.profile_inputs.fcs_type_802154.value = model.vars.fcs_type_802154.var_enum.FOUR_BYTE

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)


    def PHY_IEEE802154_SUN_915MHz_OQPSK_100kcps_NA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.SUN_OQPSK, readable_name='SUN NA-915MHz, OQPSK 100kcps',
                            phy_name=phy_name)

        phy.profile_inputs.sun_oqpsk_chiprate.value = model.vars.sun_oqpsk_chiprate.var_enum._100_KCPS

        phy.profile_inputs.base_frequency_hz.value = 902200000
        phy.profile_inputs.channel_spacing_hz.value = 200000
        phy.profile_inputs.fcs_type_802154.value = model.vars.fcs_type_802154.var_enum.FOUR_BYTE

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)


    def PHY_IEEE802154_SUN_863MHz_OQPSK_100kcps_EU(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.SUN_OQPSK, readable_name='SUN EU-863MHz, OQPSK 100kcps',
                            phy_name=phy_name)

        phy.profile_inputs.sun_oqpsk_chiprate.value = model.vars.sun_oqpsk_chiprate.var_enum._100_KCPS

        phy.profile_inputs.base_frequency_hz.value = 863100000
        phy.profile_inputs.channel_spacing_hz.value = 200000
        phy.profile_inputs.fcs_type_802154.value = model.vars.fcs_type_802154.var_enum.FOUR_BYTE

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)


    def PHY_IEEE802154_SUN_470MHz_OQPSK_100kcps_CN(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.SUN_OQPSK, readable_name='SUN CN-470MHz, OQPSK 100kcps',
                            phy_name=phy_name)

        phy.profile_inputs.sun_oqpsk_chiprate.value = model.vars.sun_oqpsk_chiprate.var_enum._100_KCPS

        phy.profile_inputs.base_frequency_hz.value = 470200000
        phy.profile_inputs.channel_spacing_hz.value = 200000
        phy.profile_inputs.fcs_type_802154.value = model.vars.fcs_type_802154.var_enum.FOUR_BYTE

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)

    def PHY_IEEE802154_SUN_915MHz_OQPSK_1000kcps_NA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.SUN_OQPSK, readable_name='SUN NA-915MHz, OQPSK 1000kcps',
                            phy_name=phy_name)

        phy.profile_inputs.sun_oqpsk_chiprate.value = model.vars.sun_oqpsk_chiprate.var_enum._1000_KCPS

        phy.profile_inputs.base_frequency_hz.value = 904000000
        phy.profile_inputs.channel_spacing_hz.value = 2000000
        phy.profile_inputs.fcs_type_802154.value = model.vars.fcs_type_802154.var_enum.FOUR_BYTE

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)