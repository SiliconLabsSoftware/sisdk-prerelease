from pyradioconfig.parts.bobcat.phys.Phys_RAIL_Base_Standard_IEEE802154 import PhysRAILBaseStandardIEEE802154Bobcat


class PhysRAILBaseStandardIEEE802154Caracal(PhysRAILBaseStandardIEEE802154Bobcat):
    
    default_xtal_frequency_hz = 39000000

    def _part_specific_phy_overrides(self, phy, model):
        pass
