from pyradioconfig.parts.bobcat.phys.Phys_Studio_IEEE802154 import PhysStudioIEEE802154Bobcat


class PhysStudioIEEE802154Rainier(PhysStudioIEEE802154Bobcat):

    default_xtal_frequency_hz = 38400000

    def PHY_IEEE802154_2p4GHz_fastswitch_prod(self, model, phy_name=None):
        pass
