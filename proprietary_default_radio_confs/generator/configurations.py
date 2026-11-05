#!/usr/bin/env python3
from pathlib import Path

project_types = {
    "Connect": "simple_connect_singlephy",
    "Base": "simple_rail_singlephy",
    "Base_Long_Preamble": "simple_long_preamble",
    "Custom": "simple_rail_multiphy",
    "Mbus": {
        True:  "wmbus_meter",
        False: "wmbus_collector"
    }
}

project_unless = {
    "simple_connect_singlephy": ["device_is_module", "device_generic_family_efr32xg21"],
    "simple_rail_singlephy": ["device_is_module", "device_generic_family_efr32xg21"],
    "simple_long_preamble": ["device_is_module", "device_generic_family_efr32xg21"],
    "simple_rail_multiphy": ["device_is_module", "device_generic_family_efr32xg21"],
    "wmbus_meter": ["device_is_module", "device_generic_family_efr32xg21"],
    "wmbus_collector": ["device_is_module", "device_generic_family_efr32xg21"]
}

device_family_names = {
    "device_generic_family_efr32xg21": "panther",
    "device_generic_family_efr32xg22": "lynx",
    "device_generic_family_efr32xg23": "ocelot",
    "device_generic_family_efr32xg24": "bobcat",
    "device_generic_family_efr32xg25": "sol",
    "device_generic_family_efr32xg26": "caracal",
    "device_generic_family_efr32xg27": "leopard",
    "device_generic_family_efr32xg28": "margay",
    "device_generic_family_efr32xg29": "lion",
    "device_generic_family_efr32xg2b": "curl",
    "device_generic_family_efr32xg2d": "serval",
    "device_generic_family_sixx301": "rainier"
}

default_xtal_frequency_hz = "39000000"

family_default_xtal_frequency_hz = {
    "device_generic_family_efr32xg21": "38400000",
    "device_generic_family_efr32xg22": "38400000",
    "device_generic_family_efr32xg23": "39000000",
    "device_generic_family_efr32xg24": "39000000",
    "device_generic_family_efr32xg25": "39000000",
    "device_generic_family_efr32xg26": "39000000",
    "device_generic_family_efr32xg27": "38400000",
    "device_generic_family_efr32xg28": "39000000",
    "device_generic_family_efr32xg29": "38400000",
    "device_generic_family_efr32xg2b": "38400000",
    "device_generic_family_efr32xg2d": "39000000",
    "device_generic_family_sixx301": "38400000"
}

project_rel_paths = {
    "devices": "platform/Device/component",
    "boards": "hardware/board/component",
    "xmls": "efr32_radio_configurator_api_factory/xml",
    "radio_configs": "pregenerated_radio_configurations",
    "restriction_file": "phy_configs.yml",
    "flex_component": ""
}


file_names = {
    "radiconf_studio_json": "radioconf_updater_input.json"
}

radioconfig_rel_paths = {
    "Base": "Base/simple_rail_singlephy",
    "Base_Long_Preamble": "Base/simple_long_preamble",
    "Custom": "Base/simple_rail_multiphy",
    "Connect": "Connect/simple_connect_singlephy",
    "Mbus": {
        "169": "Mbus/wmbus_169",
        "434": "Mbus/wmbus_434",
        "868": {
            True: "Mbus/wmbus_meter_868",
            False: "Mbus/wmbus_collector_868"
        }
    }
}

modulation_decode = {
    "DSSS8": "OQPSK",
    "2GFSK": "FSK2",
    "4GFSK": "FSK4"
}

suffixes = {
    "radio_config": ".radioconf",
    "radio_config_settings": ".json",
    "radio_config_xml": ".xml",
    "component": ".slcc",
    "restriction": ".yml"
}


class Configs(object):
    _device_family_names = device_family_names
    _radioconfig_rel_paths = radioconfig_rel_paths
    _project_rel_paths = project_rel_paths
    _generator_base_path = Path()                     # absolute path of generator base dir - set at runtime
    _project_types = project_types
    _suffixes = suffixes
    _modulation_decode = modulation_decode
    _file_names = file_names
    _project_unless = project_unless
    _default_xtal_frequency_hz = default_xtal_frequency_hz
    _family_default_xtal_frequency_hz = family_default_xtal_frequency_hz
