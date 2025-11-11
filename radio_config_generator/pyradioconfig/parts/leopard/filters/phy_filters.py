from pyradioconfig.calculator_model_framework.interfaces.iphy_filter import IPhyFilter


class PhyFilters(IPhyFilter):

    customer_phy_groups = [
                            'Phys_Internal_Base_Standard_IEEE802154',
                            'secret6',
                            'Phys_sim_tests',
                            'Phys_Internal',
                            'secret12',
                            'secret13',
                            'Phys_Utility',
                            'Phys_Internal_Base_ValOnly_aliases',
                        ]

    sim_tests_phy_groups = ['Phys_sim_tests']

    simplicity_studio_phy_groups = ['Phys_Datasheet','Phys_Connect','Phys_Studio_IEEE802154', 'Phys_Studio_BLE']

    # PHYs to exclude from regression
    virtual_phy_groups = ['Phys_Internal_Base_ValOnly_aliases']
