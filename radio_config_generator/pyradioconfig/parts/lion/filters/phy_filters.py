from pyradioconfig.calculator_model_framework.interfaces.iphy_filter import IPhyFilter


class PhyFilters(IPhyFilter):

    customer_phy_groups = [
                            'Phys_Internal_Base_Common',
                            'Phys_Internal_Base_Customer_Dexcom',
                            'Phys_Internal_Base_Customer_Imagotag',
                            'Phys_Internal_base_Customer_Quuppa',
                            'Phys_Internal_Base_Experimental',
                            'Phys_Internal_Base_Simulation',
                            'Phys_Internal_Base_Utility',
                            'Phys_Internal_Connect',
                        ]

    sim_tests_phy_groups = ['Phys_Internal_Base_Simulation', 'Phys_sim_tests']

    simplicity_studio_phy_groups = ['Phys_Datasheet','Phys_Connect','Phys_Studio_IEEE802154', 'Phys_Studio_BLE']
