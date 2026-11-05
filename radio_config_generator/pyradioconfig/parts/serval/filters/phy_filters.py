"""Serval specific filters"""

from pyradioconfig.calculator_model_framework.interfaces.iphy_filter import IPhyFilter


class PhyFilters(IPhyFilter):
    included_in_distribution_release = True

    customer_phy_groups = []

    simplicity_studio_phy_groups = [
        'Phys_Studio_Base',
        'Phys_Studio_Connect',
        'Phys_Studio_LongRange',
        'Phys_Studio_Base_Standard_SUNFSK',
        'Phys_Studio_MBus',
        'Phys_Studio_Sidewalk'
    ]

    simplicity_studio_non_ui_phy_groups = []

    sim_tests_phy_groups = []

    non_functional_phy_groups = []

    virtual_phy_groups = ['phys_studio_wisun_fan_1_1_virtual']
