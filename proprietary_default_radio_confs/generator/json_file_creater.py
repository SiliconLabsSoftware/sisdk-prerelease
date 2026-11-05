#!/usr/bin/env python3

from pathlib import Path
import json
import re
import pprint
import os

from engine import Engine
from util import Util
from configurations import Configs


class JSONFileCreater(Util, Engine):

    def create_json_file(self, list_of_all_configs):
        self.printCmdVerbose("Create new JSON file for RadioconfigManipulator")
        self.delete_previous_file()
        self.create_new_file(list_of_all_configs)
        self.printCmdSuccess("Created new JSON file for RadioconfigManipulator")

    def delete_previous_file(self):
        full_path =  Configs._generator_base_path / Configs._file_names["radiconf_studio_json"]

        if os.path.exists(str(full_path)):
            os.remove(str(full_path))
            self.printCmdVerbose("%s is deleted" % str(full_path))

    def get_protocol_name(self, input_radioconf):
        protocol_name_regex = r'<base_channel_configuration name="([^\t\n\r\f\v"]+)"'
        reg = re.compile(protocol_name_regex)
        input_file_text = ""
        input_file_path = Path(input_radioconf)
        protocol_name = "-"
        if os.path.exists(str(input_file_path)):
            with open(str(input_file_path), "r") as input_file:
                input_file_text = input_file.read()
            m = reg.search(input_file_text)

            if m is not None:
                protocol_name = m.group(1)
        else:
            self.printCmdError(f"Input file location {input_file_path} not found")
        return protocol_name

    def get_xtal_frequency_hz_for_output(self, output_radioconf):
        if output_radioconf is None:
            return None
        if "rf40mhz" in output_radioconf:
            return "40000000"
        if "rf384mhz" in output_radioconf:
            return "38400000"
        if "rf39mhz" in output_radioconf:
            return "39000000"
        return None

    def should_emit_xtal_frequency(self, xtal_frequency_hz, default_xtal_frequency_hz):
        return xtal_frequency_hz is not None and xtal_frequency_hz != default_xtal_frequency_hz

    def create_new_file(self, list_of_all_configs):
        json_dict = dict()
        json_dict.update({"inputConfigDir": ""})
        json_dict.update({"outputConfigDir": ""})

        configurations_list = list()

        for config in list_of_all_configs:
            configuration = dict()
            input_radioconf = config.get("inputConfigFile")
            protocol_name = self.get_protocol_name(input_radioconf)
            output_radioconf = config.get("outputConfigFile")
            xtal_frequency_hz = self.get_xtal_frequency_hz_for_output(output_radioconf)
            default_xtal_frequency_hz = config.get("default_xtal_frequency_hz")
            if default_xtal_frequency_hz is None:
                default_xtal_frequency_hz = Configs._default_xtal_frequency_hz
            die = config.get("die")
            profile = config.get("profile")
            phy = config.get("phy")
            channel_name = config.get("channelName")
            base_ch_frequency = config.get("base_frequency")
            channel_name_1 = config.get("channelName_1")
            channel_name_2 = config.get("channelName_2")

            phy_name_override_1 = config.get("phy_name_override_1")
            phy_name_override_2 = config.get("phy_name_override_2")

            configuration.update({"inputConfigFile": input_radioconf})
            configuration.update({"outputConfigFile": output_radioconf})
            configuration.update({"die": die})
            protocols = list()
            protocol = dict()

            if protocol_name != "-":
                protocol.update({"protocolName": protocol_name})
            protocol.update({"profile": profile})

            if phy is not None:
                protocol.update({"phy": phy})
            channels = list()
            channel = None
            channel_1 = None
            channel_2 = None

            emit_xtal_frequency_hz = self.should_emit_xtal_frequency(xtal_frequency_hz, default_xtal_frequency_hz)

            if emit_xtal_frequency_hz and channel_name is None and channel_name_1 is None and channel_name_2 is None:
                channel = dict()
                inputs = dict()
                channel.update({"channelName": "Channel Group 1"})
                inputs.update({"xtal_frequency_hz": xtal_frequency_hz})
                channel.update({"inputs": inputs})

            if channel_name is not None:
                channel = dict()
                channel.update({"channelName": channel_name})
                inputs = dict()
                if emit_xtal_frequency_hz:
                    inputs.update({"xtal_frequency_hz": xtal_frequency_hz})
                if base_ch_frequency is not None:
                    inputs.update({"base_frequency": base_ch_frequency})
                if len(inputs) >= 1:
                    channel.update({"inputs": inputs})

            if channel_name_1 is not None:
                channel_1 = dict()
                channel_1.update({"channelName": channel_name_1})
                inputs_1 = dict()
                if emit_xtal_frequency_hz:
                    inputs_1.update({"xtal_frequency_hz": xtal_frequency_hz})
                if phy_name_override_1 is not None:
                    inputs_1.update({"phy_name_override": phy_name_override_1})
                if len(inputs_1) >= 1:
                    channel_1.update({"inputs": inputs_1})

            if channel_name_2 is not None:
                channel_2 = dict()
                channel_2.update({"channelName": channel_name_2})
                inputs_2 = dict()
                if emit_xtal_frequency_hz:
                    inputs_2.update({"xtal_frequency_hz": xtal_frequency_hz})
                if phy_name_override_2 is not None:
                    inputs_2.update({"phy_name_override": phy_name_override_2})
                if len(inputs_2) >= 1:
                    channel_2.update({"inputs": inputs_2})

            if channel is not None:
                channels.append(channel)
            if channel_1 is not None:
                channels.append(channel_1)
            if channel_2 is not None:
                channels.append(channel_2)

            if len(channels) >= 1:
                protocol.update({"channels": channels})

            protocols.append(protocol)
            configuration.update({"protocols": protocols})
            configurations_list.append(configuration)

        json_dict.update({"configurations": configurations_list})

        output_json_path = Configs._generator_base_path / Configs._file_names["radiconf_studio_json"]

        with open(output_json_path, "w") as json_file:
            json.dump(json_dict, json_file, indent=2)
