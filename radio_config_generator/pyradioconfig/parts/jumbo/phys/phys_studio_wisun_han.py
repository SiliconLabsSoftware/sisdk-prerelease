from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pycalcmodel.core.common import IPCategory


class PhysStudioWisunHanJumbo(IPhy):

    ip_category = IPCategory.LPW


    def PHY_IEEE802154_WISUN_920MHz_2GFSK_50kbps_1b_JP_ECHONET(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.wisun_han,
                            readable_name='Wi-SUN ECHONET, JP-920MHz, 1b (2FSK 50kbps mi=1.0)', phy_name=phy_name)

        phy.profile_inputs.wisun_han_mode.value = model.vars.wisun_han_mode.var_enum.Mode1b
        phy.profile_inputs.wisun_reg_domain.value = model.vars.wisun_reg_domain.var_enum.JP
        phy.profile_inputs.wisun_operating_class.value = 1

        self._set_xtal_frequency(phy, xtal_freq_hz=38400000) 

        phy.profile_inputs.base_frequency_hz.value = 920600000
        phy.profile_inputs.channel_spacing_hz.value = 200000

        return phy

    def PHY_IEEE802154_WISUN_920MHz_2GFSK_100kbps_2b_JP_ECHONET(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.wisun_han,
                            readable_name='Wi-SUN ECHONET, JP-920MHz, 2b (2FSK 100kbps mi=1.0)', phy_name=phy_name)

        phy.profile_inputs.wisun_han_mode.value = model.vars.wisun_han_mode.var_enum.Mode2b
        phy.profile_inputs.wisun_reg_domain.value = model.vars.wisun_reg_domain.var_enum.JP
        phy.profile_inputs.wisun_operating_class.value = 2

        self._set_xtal_frequency(phy, xtal_freq_hz=38400000) 

        phy.profile_inputs.base_frequency_hz.value = 920900000
        phy.profile_inputs.channel_spacing_hz.value = 400000

        return phy