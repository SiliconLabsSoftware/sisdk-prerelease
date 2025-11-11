"""
Jumbo specific filters
"""
from pyradioconfig.calculator_model_framework.interfaces.iphy_filter import IPhyFilter


class PhyFilters(IPhyFilter):

    customer_phy_groups = [
                            'secret2',
                            'secret3',
                            'Phys_Bluetooth_LE',
                            'secret10',
                            'Phys_Deprecated',
                            'secret4',
                            'secret5',
                            'secret11',
                            'Phys_IEEE802154',
                            'secret6',
                            'secret7',
                            'secret8',
                            'Phys_Mbus_lab',
                            'Phys_RAIL',
                            'Phys_sim_tests',
                            'Phys_Utility',
                            'secret9',
                            'Phys_sim_tests',
                            'Phys_Internal',
                            'Phys_Internal_WiSUN',
                        ]

    sim_tests_phy_groups = ['Phys_sim_tests']

    simplicity_studio_phy_groups = ['Phys_Datasheet', 'Phys_Studio', 'Phys_connect', 'Phys_MBus_Studio',
                                    'Phys_Studio_LongRange', 'phys_studio_wisun_fan_1_0', 'phys_studio_wisun_fan_1_1',
                                    'phys_studio_wisun_fan_1_1_virtual', 'phys_studio_wisun_han', 'Phys_Studio_SUNFSK']

    non_functional_phy_groups = ['Phys_ASK']

    # PHYs to exclude from regression
    virtual_phy_groups = ['phys_studio_wisun_fan_1_1_virtual']
