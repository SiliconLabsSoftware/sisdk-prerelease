from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pyradioconfig.parts.ocelot.phys.Phys_Studio_BLE import PhysStudioBLEOcelot


class PhysInternalBaseValOnlyaliasesOcelot(IPhy):

    def PHY_Bluetooth_LE_1M_prod(self, model, phy_name=None):
        phy = PhysStudioBLEOcelot().PHY_Bluetooth_1M_prod(model, phy_name)
        return phy

    def PHY_Bluetooth_LE_2M_prod(self, model, phy_name=None):
        phy = PhysStudioBLEOcelot().PHY_Bluetooth_2M_prod(model, phy_name)
        return phy
