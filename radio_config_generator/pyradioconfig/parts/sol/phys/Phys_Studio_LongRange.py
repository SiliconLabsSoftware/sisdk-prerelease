from pyradioconfig.parts.ocelot.phys.Phys_Studio_LongRange import PHYS_OQPSK_LoRa_Ocelot
from pyradioconfig.calculator_model_framework.decorators.phy_decorators import do_not_inherit_phys

@do_not_inherit_phys
class PHYS_Studio_LongRange_Sol(PHYS_OQPSK_LoRa_Ocelot):

    def PHY_Longrange_915M_OQPSK_DSSS8_76p8kbps(self, model, phy_name=None):
        phy = super().PHY_Longrange_915M_OQPSK_DSSS8_76p8kbps(model, phy_name=phy_name)
        return phy

    def PHY_Longrange_915M_OQPSK_DSSS8_80p0kbps(self, model, phy_name=None):
        phy = super().PHY_Longrange_915M_OQPSK_DSSS8_80p0kbps(model, phy_name=phy_name)
        return phy

    def PHY_Longrange_915M_OQPSK_DSSS8_38p4kbps(self, model, phy_name=None):
        phy = super().PHY_Longrange_915M_OQPSK_DSSS8_38p4kbps(model, phy_name=phy_name)
        return phy

    def PHY_Longrange_915M_OQPSK_DSSS8_19p2kbps(self, model, phy_name=None):
        phy = super().PHY_Longrange_915M_OQPSK_DSSS8_19p2kbps(model, phy_name=phy_name)
        return phy

    def PHY_Longrange_915M_OQPSK_DSSS8_9p6kbps(self, model, phy_name=None):
        phy = super().PHY_Longrange_915M_OQPSK_DSSS8_9p6kbps(model, phy_name=phy_name)
        return phy

    def PHY_Longrange_915M_OQPSK_DSSS8_4p8kbps(self, model, phy_name=None):
        phy = super().PHY_Longrange_915M_OQPSK_DSSS8_4p8kbps(model, phy_name=phy_name)
        return phy

    def PHY_Longrange_490M_OQPSK_DSSS8_19p2kbps(self, model, phy_name=None):
        phy = super().PHY_Longrange_490M_OQPSK_DSSS8_19p2kbps(model, phy_name=phy_name)
        return phy

    def PHY_Longrange_490M_OQPSK_DSSS8_9p6kbps(self, model, phy_name=None):
        phy = super().PHY_Longrange_490M_OQPSK_DSSS8_9p6kbps(model, phy_name=phy_name)
        return phy

    def PHY_Longrange_490M_OQPSK_DSSS8_4p8kbps(self, model, phy_name=None):
        phy = super().PHY_Longrange_490M_OQPSK_DSSS8_4p8kbps(model, phy_name=phy_name)
        return phy

    def PHY_Longrange_490M_OQPSK_DSSS8_2p4kbps(self, model, phy_name=None):
        phy = super().PHY_Longrange_490M_OQPSK_DSSS8_2p4kbps(model, phy_name=phy_name)
        return phy