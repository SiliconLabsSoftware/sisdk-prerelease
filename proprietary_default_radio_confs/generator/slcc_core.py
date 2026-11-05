#!/usr/bin/env python3

import pathlib
import os
import re
import yaml

from collections import OrderedDict
from util import Util
from configurations import Configs


class SlccCore(Util):
    def __init__(self):
        self.setup_yaml()

    def delete_file(self, file_path):
        if os.path.exists(file_path):
            os.remove(file_path)

    def create_file_from(self, project_name, list_of_input):
        id = "radio_config_" + project_name
        file_name = id + ".slcc"
        file_path = Configs._generator_base_path.parent / pathlib.Path(file_name)

        self.delete_file(str(file_path))

        slcc_dict = OrderedDict()
        slcc_dict["id"] = id
        slcc_dict["label"] = "Custom Radio Configuration for %s" % " ".join(
            project_name.split("_")) + " application"
        slcc_dict["package"] = "proprietary_default_radio_confs"
        slcc_dict["description"] = "Triggers Radio Configurator UI for setting a proper (Application - Die - RF Band) Custom Radio Configurator"
        slcc_dict["category"] = "Platform|Radio Configuration"
        slcc_dict["quality"] = "production"
        slcc_dict["metadata"] = { "sbom": { "license": "Zlib" } }
        slcc_dict["provides"] = [dict(name=id), OrderedDict(name="a_radio_config", allow_multiple=True)]
        slcc_dict["requires"] = []
        slcc_dict["ui_hints"] = dict(visibility="never")
        slcc_dict["template_contribution"] = [
            OrderedDict(name="component_catalog", value=id)]
        slcc_dict["config_file"] = []

        for item in list_of_input:
            zwave_support = False
            if item.get("supports_zwave") is True and "simple_rail_singlephy" in project_name:
                zwave_support = True

            config_file_slcc_fragment = OrderedDict()
            config_file_slcc_fragment["path"] = item.get("slcc_path")
            config_file_slcc_fragment["file_id"] = 'radio_configuration_settings'
            config_file_slcc_fragment["directory"] = "rail"
            config_file_slcc_fragment["condition"] = item.get("condition")
            if item.get("unless") is not None:
                unless_list = list()
                unless_list.extend(item.get("unless"))
                if zwave_support is True:
                    unless_list.append("device_supports_zwave")
                unless_list.extend(Configs._project_unless.get(project_name))
                config_file_slcc_fragment["unless"] = unless_list
            else:
                if Configs._project_unless.get(project_name) is not None:
                    unless_list = list()
                    unless_list.extend(Configs._project_unless.get(project_name))
                    if zwave_support is True:
                        unless_list.append("device_supports_zwave")
                    config_file_slcc_fragment["unless"] = unless_list
            slcc_dict["config_file"].append(config_file_slcc_fragment)

            if zwave_support is True:
                config_file_slcc_fragment_zwave = OrderedDict()
                config_file_slcc_fragment_zwave["path"] = item.get("slcc_path")
                config_file_slcc_fragment_zwave["file_id"] = 'radio_configuration_settings'
                config_file_slcc_fragment_zwave["directory"] = "rail"
                condition_list = list()
                condition_list.extend(item.get("condition"))
                condition_list.append("device_supports_zwave")
                config_file_slcc_fragment_zwave["condition"] = condition_list
                if item.get("unless") is not None:
                    unless_list = list()
                    unless_list.extend(item.get("unless"))
                    config_file_slcc_fragment_zwave["unless"] = unless_list

                slcc_dict["config_file"].append(config_file_slcc_fragment_zwave)

        if slcc_dict.get("requires") is None:
            slcc_dict.pop("requires")
        if len(slcc_dict.get("requires")) == 0:
            slcc_dict.pop("requires")
        with open(str(file_path), "w", newline="\n") as slcc_file:
            yaml.dump(slcc_dict, slcc_file, default_flow_style=False)

    def setup_yaml(self):
        def represent_dict_order(self, data): return self.represent_mapping(
            'tag:yaml.org,2002:map', data.items())
        yaml.add_representer(OrderedDict, represent_dict_order)
