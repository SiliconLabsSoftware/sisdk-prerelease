"""
Dumbo specific filters
"""
from pyradioconfig.calculator_model_framework.interfaces.iphy_filter import IPhyFilter


class PhyFilters(IPhyFilter):
    included_in_distribution_release = True

    customer_phy_groups = []

    sim_tests_phy_groups = ['Phys_sim_tests']

    simplicity_studio_phy_groups = [
        'default_phys',
        'phy_internal_base',
        'Phys_connect',
        'Phys_MBus_Studio',
        'Phys_ASK',
        'Phys_Studio',
        'Phys_Datasheet',
    ]

    simplicity_studio_non_ui_phy_groups = [
        'Phys_IEEE802154_GB868',
        'Phys_sigfox',
    ]

    non_functional_phy_groups = ['Phys_ASK']
