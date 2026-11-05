"""
Lynx specific filters
"""
from pyradioconfig.calculator_model_framework.interfaces.iphy_filter import IPhyFilter


class PhyFilters(IPhyFilter):
    included_in_distribution_release = True

    customer_phy_groups = []

    simplicity_studio_phy_groups = [
        'Phys_Studio_IEEE802154',
        'Phys_Studio_IEEE802154_BPSK',
        'phys_studio_wisun_han',
        'Phys_Studio_Sidewalk',
        'Phys_Studio_Sigfox_TX',
        'phys_studio_wisun_fan_1_0',
        'Phys_Studio_BLE',
        'phys_studio_wisun_fan_1_1',
        'Phys_Studio_Base_Sigfox',
        'Phys_Studio_Connect',
        'Phys_Studio_Base_Standard_SUNFSK',
        'Phys_Default',
        'Phys_Studio_MBus',
        'Phys_Studio_Base',
        'Phys_Studio_LongRange',
        'phys_studio_wisun_fan_1_1_virtual'
    ]

    simplicity_studio_non_ui_phy_groups = [
        'Phys_RAIL_Base_Standard_BLE',
        'Phys_RAIL_Base_Standard_IEEE802154GB',
        'Phys_Internal_Base_Standard_IEEE802154',
        'Phys_Internal_WiSUN',
    ]

    sim_tests_phy_groups = []

    non_functional_phy_groups = []

    virtual_phy_groups = ['phys_studio_wisun_fan_1_1_virtual', 'Phys_Internal_Base_ValOnly_aliases']
