from pyradioconfig.calculator_model_framework.interfaces.iphy_filter import IPhyFilter


class PhyFilters(IPhyFilter):
    included_in_distribution_release = True

    customer_phy_groups = []

    sim_tests_phy_groups = []

    simplicity_studio_phy_groups = [
        'Phys_Studio_Base',
        'Phys_Studio_Connect',
        'Phys_Studio_IEEE802154',
        'Phys_Studio_BLE'
    ]

    simplicity_studio_non_ui_phy_groups = [
        'Phys_RAIL_Base_Standard_BLE',
        'Phys_RAIL_Base_Standard_BLE_HADM',
        'Phys_RAIL_Base_Standard_IEEE802154',
    ]

    virtual_phy_groups = ['Phys_Internal_Base_ValOnly_aliases']
