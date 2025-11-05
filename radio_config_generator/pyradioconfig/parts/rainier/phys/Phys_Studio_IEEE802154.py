from pyradioconfig.parts.bobcat.phys.Phys_Studio_IEEE802154 import PhysStudioIEEE802154Bobcat


class PhysStudioIEEE802154Rainier(PhysStudioIEEE802154Bobcat):

    def PHY_IEEE802154_2p4GHz_prod(self, model, phy_name=None):
        phy = super().PHY_IEEE802154_2p4GHz_prod(model, phy_name)
        phy.profile_inputs.xtal_frequency_hz.value = 38400000
        return phy

    def PHY_IEEE802154_2p4GHz_fem_prod(self, model, phy_name=None):
        phy = super().PHY_IEEE802154_2p4GHz_fem_prod(model, phy_name)
        phy.profile_inputs.xtal_frequency_hz.value = 38400000
        return phy

    def PHY_IEEE802154_2p4GHz_diversity_prod(self, model, phy_name=None):
        phy = super().PHY_IEEE802154_2p4GHz_diversity_prod(model, phy_name)
        phy.profile_inputs.xtal_frequency_hz.value = 38400000
        return phy

    def PHY_IEEE802154_2p4GHz_diversity_fem_prod(self, model, phy_name=None):
        phy = super().PHY_IEEE802154_2p4GHz_diversity_fem_prod(model, phy_name)
        phy.profile_inputs.xtal_frequency_hz.value = 38400000
        return phy

    def PHY_IEEE802154_2p4GHz_fastswitch_prod(self, model, phy_name=None):
        pass
