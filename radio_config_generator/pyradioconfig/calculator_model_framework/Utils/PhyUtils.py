from pyradioconfig.calculator_model_framework.CalcManager import CalcManager


def find_all_nonsim_phys(part_family, part_rev='ANY', target=CalcManager.IC_str):

    radio_configurator = CalcManager(part_family, part_rev, target=target)
    types_model = radio_configurator.create_modem_model_type()

    sim_tests_phy_groups = radio_configurator.get_sim_tests_phy_groups()
    filtered_phys = radio_configurator.filter_out_phy_group_names(types_model, sim_tests_phy_groups)

    target_tag = radio_configurator.getTargetTag()
    phy_list = []
    for phy in filtered_phys:
        phy_supported_on_target = radio_configurator.check_phy_supported_on_target(phy.name, target_tag=target_tag, model=types_model)
        if phy_supported_on_target:
            phy_list.append(phy.name)

    return phy_list
