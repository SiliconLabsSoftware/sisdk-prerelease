from pyradioconfig.calculator_model_framework.decorators.phy_decorators import do_not_inherit_phys
from pyradioconfig.parts.ocelot.phys.Phys_Internal_Base_Standard_IEEE802154 import \
    PhysInternalBaseStandardIEEE802154Ocelot


@do_not_inherit_phys
class PhysInternalBaseStandardIEEE802154Serval(PhysInternalBaseStandardIEEE802154Ocelot):

    def PHY_IEEE802154_868MHz_OQPSK_coh(self, model, phy_name=None):
        phy = super().PHY_IEEE802154_868MHz_OQPSK_coh(model, phy_name=phy_name)
        return phy
