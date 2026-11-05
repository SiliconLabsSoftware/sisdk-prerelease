#!/usr/bin/env python3

from pathlib import Path
import yaml
import re
import xml.etree.ElementTree as ET
from util import Util
from engine import Engine
from configurations import Configs


class XMLDatabaseBuilder(Util, Engine):

    def build_profiles_database(self):
        self.printCmdInfo("Building XML database")
        profiles_by_family = self.read_xml_files()
        if len(profiles_by_family) > 0:
            self.printCmdSuccess("Building XML database finished")
        else:
            self.printCmdError("Building XML database finished")
        return profiles_by_family

    def read_xml_files(self):
        profiles_by_family = dict()

        full_path = Configs._generator_base_path / Configs._project_rel_paths["xmls"]

        count_of_files = len(list(full_path.glob('*'+Configs._suffixes["radio_config_xml"])))

        self.printProgressBar(
            0, count_of_files, prefix='Progress:', suffix='Complete', length=50)

        files_read = 0
        for file in full_path.iterdir():
            if file.is_file():
                if file.suffix == Configs._suffixes.get("radio_config_xml"):
                    part_family, profiles = self.process_xml_file(file)
                    family_codename = self.get_codename_from_family_name(part_family)   # eg. device_generic_family_xg2d --> serval
                    profiles_by_family.update({family_codename: profiles})

                    files_read = files_read + 1
                    self.printProgressBar(
                        files_read, count_of_files, prefix='Progress:', suffix='Complete', length=50)
        return profiles_by_family

    def process_xml_file(self, path):
        # read xml file
        tree = ET.parse(path)
        root = tree.getroot()

        profile_dict = dict()

        part_family = root.attrib.get("part_family")
        # print(f"{part_family}")

        # update project with all supported boards
        for phys in root.findall("phys"):
            for phy in phys.findall("phy"):
                pyh_name = phy.attrib.get('name')
                phy_profile = phy.attrib.get('profile_name')
                # print(f"{phy.attrib.get('name')} {phy.attrib.get('profile_name')}")
                bitrate = None
                base_frequency_hz = None
                modulation_type = None
                for profile_inputs in phy.findall("profile_inputs"):
                    for profile_input in profile_inputs.findall("profile_input"):
                        if profile_input.find("var_name").text == "base_frequency_hz":
                            base_frequency_hz = profile_input.find(
                                "values").find("value").text
                            # print(f"base_frequency_hz = {base_frequency_hz}")
                        if profile_input.find("var_name").text == "bitrate":
                            bitrate = profile_input.find(
                                "values").find("value").text
                            # print(f"bitrate = {bitrate}")
                        if profile_input.find("var_name").text == "modulation_type":
                            modulation_type = profile_input.find(
                                "values").find("value").text
                            # print(f"bitrate = {bitrate}")
                if base_frequency_hz is not None:

                    if bitrate is None:
                        pyh_name_list = pyh_name.split("_")
                        for parts in pyh_name_list:
                            if "bps" in parts:
                                bitrate = self.decode_bitrare(parts)
                    if bitrate is None and phy_profile != "Mbus":
                        continue

                    if modulation_type is None and phy_profile != "Mbus":
                        modulation_type = self.decode_modulation(pyh_name)

                    pyh_attr_dict = dict()
                    pyh_attr_dict.update(
                        {"base_frequency_hz": base_frequency_hz})
                    pyh_attr_dict.update({"bitrate": bitrate})
                    if phy_profile != "Mbus":
                        pyh_attr_dict.update(
                            {"modulation_type": modulation_type.split('.')[1]})
                    else:
                        pyh_attr_dict.update(
                            {"modulation_type": "mbus"})

                    base_dict = profile_dict.get(phy_profile)
                    if base_dict is None:
                        base_dict = dict()
                    base_dict.update({pyh_name: pyh_attr_dict})
                    profile_dict.update({phy_profile: base_dict})
        return (part_family, profile_dict)

    def decode_bitrare(self, parts):
        bitrate = None
        regex = r'([0-9]+)([p]?)([0-9]?)([KkMm]?)'
        matching = re.match(regex, parts)
        if matching is not None:
            value_upper = matching.group(1)
            value_p = matching.group(2)
            value_lower = matching.group(3)
            value_multiplyer = matching.group(4)
            value = float(value_upper)
            if value_p == "p":
                p_lower_value = float(value_lower)
                value = value + (p_lower_value/10)
            if value_multiplyer in ["K", "k"]:
                value = value * 1000
            if value_multiplyer in ["M", "m"]:
                value = value * 1000 * 1000
            bitrate = str(int(value))
        return bitrate

    def decode_modulation(self, pyh_name):
        pyh_name_list = pyh_name.split("_")
        poz = 0
        for phy_part in pyh_name_list:
            poz = poz + 1
            if "bps" in phy_part:
                poz = poz - 2
                break
        modulation = pyh_name_list[poz]
        dec_modulation = Configs._modulation_decode.get(modulation)
        if dec_modulation is None:
            dec_modulation = modulation

        return "ModModeEnum." + dec_modulation
