from pyradioconfig.parts.bobcat.phys.Phys_Studio_Base import PHYS_Studio_Base_Bobcat
from pyradioconfig.calculator_model_framework.decorators.phy_decorators import do_not_inherit_phys

@do_not_inherit_phys
class Phy_Studio_Base_Curl(PHYS_Studio_Base_Bobcat):

    """
        https://jira.silabs.com/browse/MCUW_RADIO_CFG-3358
        Define the Datasheet PHYs for curl based on bobcat.
        Those PHY are based on Studio Profile Base profile.
    """    

    def PHY_Studio_2450M_2GFSK_1Mbps_500K(self, model, phy_name=None):
        phy = super().PHY_Studio_2450M_2GFSK_1Mbps_500K(model, phy_name=phy_name)

        # Chenge crystal frequency
        phy.profile_inputs.xtal_frequency_hz.value = 40000000

        return phy


    def PHY_Studio_2450M_2GFSK_250Kbps_125K(self, model, phy_name=None):
        phy = super().PHY_Studio_2450M_2GFSK_250Kbps_125K(model, phy_name=phy_name)

        # Chenge crystal frequency
        phy.profile_inputs.xtal_frequency_hz.value = 40000000

        return phy


    def PHY_Studio_2450M_2GFSK_2Mbps_1M(self, model, phy_name=None):
        phy = super().PHY_Studio_2450M_2GFSK_2Mbps_1M(model, phy_name=phy_name)

        # Chenge crystal frequency
        phy.profile_inputs.xtal_frequency_hz.value = 40000000

        return phy

