from pyradioconfig.calculator_model_framework.interfaces.iphy_filter import IPhyFilter


class PhyFilters_Sol(IPhyFilter):
    included_in_distribution_release = True

    customer_phy_groups = []

    simplicity_studio_phy_groups = [
        'Phys_Studio_IEEE802154',
        'Phys_Studio_SUN_OQPSK',
        'Phys_Studio_Base',
        'Phys_Studio_Sidewalk',
        'Phys_Studio_Connect',
        'phys_studio_wisun_han',
        'Phys_Studio_LongRange',
        'Phys_Studio_Base_Standard_SUNFSK',
        'phys_studio_wisun_fan_1_0',
        'phys_studio_wisun_fan_1_1',
        'phys_studio_wisun_fan_1_1_virtual',
        'Phys_Studio_Connect_OFDM',
    ]

    simplicity_studio_non_ui_phy_groups = [
        'Phys_RAIL_Base_Standard_BLE',
        'Phys_RAIL_Base_Standard_IEEE802154GB',
    ]

    sim_tests_phy_groups = []

    non_functional_phy_groups = []

    virtual_phy_groups = ['Phys_Internal_WiSUN_OFDM_MCSx', 'phys_studio_wisun_fan_1_1_virtual']
