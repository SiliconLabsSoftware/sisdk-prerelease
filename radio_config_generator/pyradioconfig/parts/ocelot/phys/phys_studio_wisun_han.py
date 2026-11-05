from pyradioconfig.calculator_model_framework.decorators.phy_decorators import do_not_inherit_phys
from pyradioconfig.parts.jumbo.phys.phys_studio_wisun_han import PhysStudioWisunHanJumbo

@do_not_inherit_phys
class PhysStudioWisunHanOcelot(PhysStudioWisunHanJumbo):


    def PHY_IEEE802154_WISUN_920MHz_2GFSK_50kbps_1b_JP_ECHONET(self, model, phy_name=None):
        phy = super().PHY_IEEE802154_WISUN_920MHz_2GFSK_50kbps_1b_JP_ECHONET(model)

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)

        return phy

    def PHY_IEEE802154_WISUN_920MHz_2GFSK_100kbps_2b_JP_ECHONET(self, model, phy_name=None):
        phy = super().PHY_IEEE802154_WISUN_920MHz_2GFSK_100kbps_2b_JP_ECHONET(model)

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)

        return phy

