"""
Dumbo specific filters
"""
from pyradioconfig.calculator_model_framework.interfaces.iphy_filter import IPhyFilter


class PhyFilters(IPhyFilter):

    customer_phy_groups = [
                            'secret2',
                            'secret3',
                            'Phys_Bluetooth_LE',
                            'Phys_Deprecated',
                            'secret4',
                            'secret5',
                            'Phys_IEEE802154',
                            'secret6',
                            'secret7',
                            'secret8',
                            'Phys_Mbus_lab',
                            'Phys_OOK',
                            'Phys_RAIL',
                            'Phys_sim_tests',
                            'Phys_Utility',
                            'secret9',
                            'Phys_sim_tests',
                            'Phys_Internal',
                        ]

    sim_tests_phy_groups = ['Phys_sim_tests']

    simplicity_studio_phy_groups = ['Phys_Datasheet', 'Phys_Studio', 'Phys_connect', 'Phys_MBus_Studio' ]

    non_functional_phy_groups = ['Phys_ASK']
