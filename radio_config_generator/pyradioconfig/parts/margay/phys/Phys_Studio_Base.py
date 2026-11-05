from pyradioconfig.parts.ocelot.phys.Phys_Studio_Base import PHYS_Studio_Base_Ocelot
from pyradioconfig.parts.bobcat.phys.Phys_Studio_Base import PHYS_Studio_Base_Bobcat


class PHYS_Studio_Base_Margay(PHYS_Studio_Base_Ocelot):
    def PHY_Studio_2450M_2GFSK_1Mbps_500K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='2450M 2GFSK 1Mbps 500K', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 1000000
        phy.profile_inputs.deviation.value = 500000

        phy.profile_inputs.base_frequency_hz.value = 2450000000

        return phy

    def PHY_Studio_2450M_2GFSK_250Kbps_125K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='2450M 2GFSK 250Kbps 125K', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 250000
        phy.profile_inputs.deviation.value = 125000

        phy.profile_inputs.base_frequency_hz.value = 2450000000

        return phy

    def PHY_Studio_2450M_2GFSK_2Mbps_1M(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='2450M 2GFSK 2Mbps 1M', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 2000000
        phy.profile_inputs.deviation.value = 1000000

        phy.profile_inputs.base_frequency_hz.value = 2450000000

        return phy
    pass