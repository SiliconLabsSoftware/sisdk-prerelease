#!/usr/bin/env python3

import pathlib
import json
import re
import pprint
from util import Util
from configurations import Configs


def get_folder_name(profile, band, is_meter):
    out_path = None
    if isinstance(Configs._radioconfig_rel_paths[profile], str):
        out_path = Configs._radioconfig_rel_paths[profile]
    elif isinstance(Configs._radioconfig_rel_paths[profile][band], str):
        out_path = Configs._radioconfig_rel_paths[profile][band]
    else:
        out_path = Configs._radioconfig_rel_paths[profile][band][is_meter]

    if out_path is not None:
        return Configs._project_rel_paths["flex_component"] + out_path
    else:
        return out_path


def get_default_sub_giga(default_bands):
    if "hardware_board_default_rf_band_868" in default_bands:
        return "hardware_board_default_rf_band_868"
    elif "hardware_board_default_rf_band_915" in default_bands:
        return "hardware_board_default_rf_band_915"
    elif "hardware_board_default_rf_band_434" in default_bands:
        return "hardware_board_default_rf_band_434"
    elif "hardware_board_default_rf_band_169" in default_bands:
        return "hardware_board_default_rf_band_169"
    elif "hardware_board_default_rf_band_490" in default_bands:
        return "hardware_board_default_rf_band_490"
    else:
        return list(default_bands)[0]


def get_wmbus_default_phy(default_bands):
    if "hardware_board_default_rf_band_868" in default_bands:
        return "hardware_board_default_rf_band_868"
    elif "hardware_board_default_rf_band_434" in default_bands:
        return "hardware_board_default_rf_band_434"
    elif "hardware_board_default_rf_band_169" in default_bands:
        return "hardware_board_default_rf_band_169"
    else:
        return None


class OutputContentGenerator(Util):

    multi_phy_slcc_path = set()

    def get_default_xtal_frequency_hz(self, device_family):
        return Configs._family_default_xtal_frequency_hz.get(device_family, Configs._default_xtal_frequency_hz)


    def get_in_file_path(self, radio_file_dict, profile, band, device_sdid, is_sub_giga, is_meter):
        if profile == "Base":
            return self.get_path_for_radioconf(radio_file_dict, "simple_rail_singlephy", device_sdid)
        elif profile == "Base_Long_Preamble":
            return self.get_path_for_radioconf(radio_file_dict, "simple_long_preamble", device_sdid)
        elif profile == "Custom":
            if is_sub_giga is True:
                return self.get_path_for_radioconf(radio_file_dict, "simple_rail_multiphy_sub", device_sdid)
            else:
                return self.get_path_for_radioconf(radio_file_dict, "simple_rail_multiphy", device_sdid)
        elif profile == "Connect":
            return self.get_path_for_radioconf(radio_file_dict, "simple_connect_singlephy", device_sdid)
        elif profile == "Mbus":
            if band == "169":
                return self.get_path_for_radioconf(radio_file_dict, "wmbus_169", device_sdid)
            elif band == "434":
                return self.get_path_for_radioconf(radio_file_dict, "wmbus_434", device_sdid)
            elif band == "868":
                if is_meter is True:
                    return self.get_path_for_radioconf(radio_file_dict, "wmbus_meter_868", device_sdid)
                else:
                    return self.get_path_for_radioconf(radio_file_dict, "wmbus_collector_868", device_sdid)
            else:
                return None
        elif profile == "WiSUN":
            return self.get_path_for_radioconf(radio_file_dict, "simple_wisun_singlephy", device_sdid)
        else:
            return None

    def get_phy_profile(self, profile):
        if profile == "Custom":
            return "Base"
        else:
            return profile

    def generate_board_out_content(self, project, hw_db, reduced_database, radio_dict, rest_dict=dict(), is_meter=False):
        if project in rest_dict.keys():
            return self.generate_single_phy_out_dict_content(project, hw_db, reduced_database, radio_dict)
        elif project == "Mbus":
            return self.generate_mbus_phy_out_dict_content(hw_db, reduced_database, radio_dict, is_meter)
        elif project == "Custom":
            return self.generate_multi_phy_out_dict_content(hw_db, reduced_database, radio_dict)
        elif project == "Base_Long_Preamble":
            return self.generate_single_phy_out_dict_content(project, hw_db, reduced_database, radio_dict)
        else:
            return list()

    def generate_custom_out_content(self, project, die_dict, reduced_database, rest_dict=dict(), is_meter=False):
        if project in rest_dict.keys():
            return self.generate_custom_single_content(project, die_dict, reduced_database)
        elif project == "Mbus":
            return self.generate_custom_wmbus_content(die_dict, is_meter, reduced_database)
        elif project == "Custom":
            return self.generate_custom_multipy_content(die_dict)
        elif project == "Base_Long_Preamble":
            return self.generate_custom_single_content(project, die_dict, reduced_database)
        else:
            return list()

    def generate_single_phy_out_dict_content(self, profile, hw_db, reduced_database, radio_file_dict):
        out_list = list()

        for die_name in hw_db.keys():
            if("device_generic_family_efr32xg21" in die_name):
                continue # skip panther as ther is no radio config
            die_data = hw_db.get(die_name)
            default_xtal_frequency_hz = self.get_default_xtal_frequency_hz(die_name)
            supports_zwave = die_data.get("supports_zwave")
            if supports_zwave is None:
                supports_zwave = False
            default_bands = die_data.get("default_bands")
            has_40mhz_variant = die_data.get("has_40mhz_variant")
            has_38p4mhz_variant = die_data.get("has_38p4mhz_variant")
            has_39mhz_variant = die_data.get("has_39mhz_variant")

            for default_band in default_bands:
                if reduced_database.get(die_name) is None:
                    continue
                if reduced_database.get(die_name).get(profile.split("_")[0]) is None:
                    continue
                phy = reduced_database.get(die_name).get(
                    profile.split("_")[0]).get(default_band)
                if phy is None:
                    continue

                band = "rf_band_" + self.get_rf_band(default_band)
                phy_profile = self.get_phy_profile(profile.split("_")[0])


                if has_40mhz_variant:
                    unless= list()
                    unless.append("hardware_board_has_hfxo_freq_40mhz")
                    if has_38p4mhz_variant:
                        unless.append("hardware_board_has_hfxo_freq_38.4mhz")
                    if has_39mhz_variant:
                        unless.append("hardware_board_has_hfxo_freq_39mhz")
                if has_38p4mhz_variant:
                    unless= list()
                    unless.append("hardware_board_has_hfxo_freq_38.4mhz")
                    if has_40mhz_variant:
                        unless.append("hardware_board_has_hfxo_freq_40mhz")
                    if has_39mhz_variant:
                        unless.append("hardware_board_has_hfxo_freq_39mhz")
                if has_39mhz_variant:
                    unless= list()
                    unless.append("hardware_board_has_hfxo_freq_39mhz")
                    if has_40mhz_variant:
                        unless.append("hardware_board_has_hfxo_freq_40mhz")
                    if has_38p4mhz_variant:
                        unless.append("hardware_board_has_hfxo_freq_38.4mhz")
                if get_folder_name(profile, self.get_rf_band(default_band), False) is None:
                    continue
                folder = get_folder_name(profile, self.get_rf_band(default_band), False) + "/" + die_name + "/" + band
                in_radio_conf_file = self.get_in_file_path(
                    radio_file_dict, profile, self.get_rf_band(default_band), die_name, False, False)
                slcc_path = folder + "/" + "radio_settings.radioconf"
                out_radio_conf_file = Configs._generator_base_path.parent / slcc_path

                condition = list()
                condition.append(default_band)
                condition.append(die_name)

                config_dict = dict()

                config_dict.update(
                    {"inputConfigFile": str(in_radio_conf_file)})
                config_dict.update(
                    {"outputConfigFile": self.get_true_path(out_radio_conf_file)})
                config_dict.update({"slcc_path": slcc_path})
                config_dict.update({"die": self.get_die_from_sdid(die_name)})
                config_dict.update({"sdid": die_name})
                config_dict.update({"profile": phy_profile})
                config_dict.update({"phy": phy})
                config_dict.update({"condition": condition})
                config_dict.update({"default_xtal_frequency_hz": default_xtal_frequency_hz})
                if has_40mhz_variant:
                    config_dict.update({"unless": unless})
                if has_38p4mhz_variant:
                    config_dict.update({"unless": unless})
                if has_39mhz_variant:
                    config_dict.update({"unless": unless})
                config_dict.update({"supports_zwave": supports_zwave})

                out_list.append(config_dict)

                if has_40mhz_variant:
                    folder_40Mhz = folder + "/rf40mhz"
                    slcc_path_40Mhz = folder_40Mhz + "/" + "radio_settings.radioconf"
                    out_radio_conf_file_40MHz = Configs._generator_base_path.parent / slcc_path_40Mhz

                    condition_40MHz = list()
                    condition_40MHz.extend(condition)
                    condition_40MHz.append("hardware_board_has_hfxo_freq_40mhz")

                    config_dict_40Mhz = dict()

                    config_dict_40Mhz.update(
                        {"inputConfigFile": str(in_radio_conf_file)})
                    config_dict_40Mhz.update(
                        {"outputConfigFile": self.get_true_path(out_radio_conf_file_40MHz)})
                    config_dict_40Mhz.update({"slcc_path": slcc_path_40Mhz})
                    config_dict_40Mhz.update({"die": self.get_die_from_sdid(die_name)})
                    config_dict_40Mhz.update({"sdid": die_name})
                    config_dict_40Mhz.update({"profile": phy_profile})
                    config_dict_40Mhz.update({"phy": phy})
                    config_dict_40Mhz.update({"condition": condition_40MHz})
                    config_dict_40Mhz.update({"default_xtal_frequency_hz": default_xtal_frequency_hz})
                    config_dict_40Mhz.update({"supports_zwave": supports_zwave})

                    out_list.append(config_dict_40Mhz)

                if has_38p4mhz_variant:
                    folder_384Mhz = folder + "/rf384mhz"
                    slcc_path_384Mhz = folder_384Mhz + "/" + "radio_settings.radioconf"
                    out_radio_conf_file_384MHz = Configs._generator_base_path.parent / slcc_path_384Mhz

                    condition_384MHz = list()
                    condition_384MHz.extend(condition)
                    condition_384MHz.append("hardware_board_has_hfxo_freq_38.4mhz")

                    config_dict_384MHz = dict()
                    config_dict_384MHz.update(
                        {"inputConfigFile": str(in_radio_conf_file)})
                    config_dict_384MHz.update(
                        {"outputConfigFile": self.get_true_path(out_radio_conf_file_384MHz)})
                    config_dict_384MHz.update({"slcc_path": slcc_path_384Mhz})
                    config_dict_384MHz.update({"die": self.get_die_from_sdid(die_name)})
                    config_dict_384MHz.update({"sdid": die_name})
                    config_dict_384MHz.update({"profile": phy_profile})
                    config_dict_384MHz.update({"phy": phy})
                    config_dict_384MHz.update({"condition": condition_384MHz})
                    config_dict_384MHz.update({"default_xtal_frequency_hz": default_xtal_frequency_hz})
                    config_dict_384MHz.update({"supports_zwave": supports_zwave})


                    out_list.append(config_dict_384MHz)

                if has_39mhz_variant:
                    folder_39Mhz = folder + "/rf39mhz"
                    slcc_path_39Mhz = folder_39Mhz + "/" + "radio_settings.radioconf"
                    out_radio_conf_file_39MHz = Configs._generator_base_path.parent / slcc_path_39Mhz

                    condition_39MHz = list()
                    condition_39MHz.extend(condition)
                    condition_39MHz.append("hardware_board_has_hfxo_freq_39mhz")

                    config_dict_39MHz = dict()

                    config_dict_39MHz.update(
                        {"inputConfigFile": str(in_radio_conf_file)})
                    config_dict_39MHz.update(
                        {"outputConfigFile": self.get_true_path(out_radio_conf_file_39MHz)})
                    config_dict_39MHz.update({"slcc_path": slcc_path_39Mhz})
                    config_dict_39MHz.update({"die": self.get_die_from_sdid(die_name)})
                    config_dict_39MHz.update({"sdid": die_name})
                    config_dict_39MHz.update({"profile": phy_profile})
                    config_dict_39MHz.update({"phy": phy})
                    config_dict_39MHz.update({"condition": condition_39MHz})
                    config_dict_39MHz.update({"default_xtal_frequency_hz": default_xtal_frequency_hz})
                    config_dict_39MHz.update({"supports_zwave": supports_zwave})

                    out_list.append(config_dict_39MHz)


        return sorted(out_list, key=lambda i: (i['sdid'], i['slcc_path']))

    def generate_mbus_phy_out_dict_content(self, die_dict, reduced_database, radio_file_dict, is_meter):
        out_list = list()

        profile = "Mbus"

        for die_name in die_dict.keys():
            if("device_generic_family_efr32xg21" in die_name):
                continue # skip panther as ther is no radio config
            die_data = die_dict.get(die_name)
            default_xtal_frequency_hz = self.get_default_xtal_frequency_hz(die_name)
            default_bands = die_data.get("default_bands")
            for default_band in default_bands:
                if default_band not in ["hardware_board_default_rf_band_169", "hardware_board_default_rf_band_434", "hardware_board_default_rf_band_868"]:
                    continue
                if reduced_database.get(die_name) is None:
                    continue
                if reduced_database.get(die_name).get(profile) is None:
                    continue

                band = "rf_band_" + self.get_rf_band(default_band)
                phy_profile = self.get_phy_profile(profile)
                folder = get_folder_name(profile, self.get_rf_band(
                    default_band), is_meter) + "/" + die_name + "/" + band
                in_radio_conf_file = self.get_in_file_path(
                    radio_file_dict, profile, self.get_rf_band(default_band), die_name, False, is_meter)
                slcc_path = folder + "/" + "radio_settings.radioconf"
                out_radio_conf_file = Configs._generator_base_path.parent / slcc_path

                condition = list()
                condition.append(default_band)
                condition.append(die_name)

                config_dict = dict()

                config_dict.update(
                    {"inputConfigFile": str(in_radio_conf_file)})
                config_dict.update(
                    {"outputConfigFile": self.get_true_path(out_radio_conf_file)})
                config_dict.update({"slcc_path": slcc_path})
                config_dict.update({"die": self.get_die_from_sdid(die_name)})
                config_dict.update({"sdid": die_name})
                config_dict.update({"profile": phy_profile})
                config_dict.update({"condition": condition})
                config_dict.update({"default_xtal_frequency_hz": default_xtal_frequency_hz})

                out_list.append(config_dict)
        return sorted(out_list, key=lambda i: (i['sdid'], i['slcc_path']))

    def generate_multi_phy_out_dict_content(self, die_dict, reduced_database, radio_file_dict):
        out_list = list()

        for die_name in die_dict.keys():
            if("device_generic_family_efr32xg21" in die_name):
                continue # skip panther as ther is no radio config
            die_data = die_dict.get(die_name)
            default_xtal_frequency_hz = self.get_default_xtal_frequency_hz(die_name)
            default_bands = die_data.get("default_bands")
            multiphy_combinations = die_data.get("multiphy_combinations")
            has_40mhz_variant = die_data.get("has_40mhz_variant")
            has_38p4mhz_variant = die_data.get("has_38p4mhz_variant")
            has_39mhz_variant = die_data.get("has_39mhz_variant")
            if multiphy_combinations is None:
                continue

            if len(multiphy_combinations) < 1:
                continue

            comb_dict = dict()
            for combination in multiphy_combinations:
                [first_band, second_band] = combination.split(":")
                if comb_dict.get(first_band) is None:
                    comb_dict.update({first_band: [second_band]})
                else:
                    previous_list = comb_dict.get(first_band)
                    previous_list.append(second_band)
                    comb_dict.update({first_band: previous_list})

            for default_band in default_bands:
                phy = "Custom"
                band = "rf_band_" + self.get_rf_band(default_band)
                phy_profile = self.get_phy_profile(phy)

                if comb_dict.get(default_band) is None:
                    continue
                for second_rf in comb_dict.get(default_band):

                    phy_1 = reduced_database.get(die_name).get(phy_profile.split("_")[0]).get(default_band)

                    second_rf_int = int(self.get_rf_band(second_rf))
                    if second_rf_int > 1000:
                        is_sub_giga = False
                        folder = get_folder_name(phy, self.get_rf_band(
                            default_band), False) + "/" + die_name + "/" + band + "/2p4GHz"
                        in_radio_conf_file = self.get_in_file_path(radio_file_dict, phy, self.get_rf_band(default_band),
                                                                   die_name, is_sub_giga, False)
                        phy_2 = reduced_database.get(die_name).get(phy_profile.split("_")[0]).get("hardware_board_default_rf_band_"+str(second_rf_int))
                    else:
                        is_sub_giga = True
                        folder = get_folder_name(phy, self.get_rf_band(
                            default_band), False) + "/" + die_name + "/" + band + "/subGHz/" + "rf_band_" + self.get_rf_band(second_rf)
                        in_radio_conf_file = self.get_in_file_path(radio_file_dict, phy, self.get_rf_band(default_band),
                                                                   die_name, is_sub_giga, False)
                        if second_rf_int == 914:
                            second_rf_int = 915
                        if die_name == "device_generic_family_efr32xg23" and second_rf_int == 915:
                            phy_2 = "PHY_Studio_915M_2GFSK_100Kbps_50K_antdiv"
                        else:
                            phy_2 = reduced_database.get(die_name).get(phy_profile.split("_")[0]).get("hardware_board_default_rf_band_"+ str(second_rf_int))

                    if has_40mhz_variant:
                        unless= list()
                        unless.append("hardware_board_has_hfxo_freq_40mhz")
                        if has_38p4mhz_variant:
                            unless.append("hardware_board_has_hfxo_freq_38.4mhz")
                        if has_39mhz_variant:
                            unless.append("hardware_board_has_hfxo_freq_39mhz")
                    if has_38p4mhz_variant:
                        unless= list()
                        unless.append("hardware_board_has_hfxo_freq_38.4mhz")
                        if has_40mhz_variant:
                            unless.append("hardware_board_has_hfxo_freq_40mhz")
                        if has_39mhz_variant:
                            unless.append("hardware_board_has_hfxo_freq_39mhz")
                    if has_39mhz_variant:
                        unless= list()
                        unless.append("hardware_board_has_hfxo_freq_39mhz")
                        if has_40mhz_variant:
                            unless.append("hardware_board_has_hfxo_freq_40mhz")
                        if has_38p4mhz_variant:
                            unless.append("hardware_board_has_hfxo_freq_38.4mhz")
                    slcc_path = folder + "/" + "radio_settings.radioconf"
                    out_radio_conf_file = Configs._generator_base_path.parent / slcc_path

                    self.multi_phy_slcc_path.add(slcc_path)

                    condition = list()
                    condition.append(default_band)
                    condition.append(die_name)
                    condition.append(second_rf)

                    config_dict = dict()

                    config_dict.update(
                        {"inputConfigFile": str(in_radio_conf_file)})
                    config_dict.update(
                        {"outputConfigFile": self.get_true_path(out_radio_conf_file)})
                    config_dict.update({"slcc_path": slcc_path})
                    config_dict.update(
                        {"die": self.get_die_from_sdid(die_name)})
                    config_dict.update({"sdid": die_name})
                    config_dict.update({"profile": phy_profile})
                    config_dict.update({"phy": phy_1})
                    config_dict.update({"condition": condition})
                    config_dict.update({"default_xtal_frequency_hz": default_xtal_frequency_hz})
                    if has_40mhz_variant:
                        config_dict.update({"unless": unless})
                    if has_38p4mhz_variant:
                        config_dict.update({"unless": unless})
                    if has_39mhz_variant:
                        config_dict.update({"unless": unless})

                    config_dict.update(
                            {"phy_name_override_1": phy_1})
                    config_dict.update(
                            {"phy_name_override_2": phy_2})

                    if is_sub_giga:
                        config_dict.update(
                            {"channelName_1": "first_channelSubGHz"})
                        config_dict.update(
                            {"channelName_2": "second_channelSubGHz"})
                    else:
                        config_dict.update(
                            {"channelName_1": "channelSubGHz"})
                        config_dict.update(
                            {"channelName_2": "channel2p4"})

                    out_list.append(config_dict)

                    if has_40mhz_variant:
                        folder_40Mhz = folder + "/rf40mhz"
                        slcc_path_40Mhz = folder_40Mhz + "/" + "radio_settings.radioconf"
                        out_radio_conf_file_40MHz = Configs._generator_base_path.parent / slcc_path_40Mhz

                        condition_40MHz = list()
                        condition_40MHz.extend(condition)
                        condition_40MHz.append("hardware_board_has_hfxo_freq_40mhz")

                        config_dict_40Mhz = dict()

                        config_dict_40Mhz.update(
                            {"inputConfigFile": str(in_radio_conf_file)})
                        config_dict_40Mhz.update(
                            {"outputConfigFile": self.get_true_path(out_radio_conf_file_40MHz)})
                        config_dict_40Mhz.update({"slcc_path": slcc_path_40Mhz})
                        config_dict_40Mhz.update(
                            {"die": self.get_die_from_sdid(die_name)})
                        config_dict_40Mhz.update({"sdid": die_name})
                        config_dict_40Mhz.update({"profile": phy_profile})
                        config_dict_40Mhz.update({"phy": phy_1})
                        config_dict_40Mhz.update({"condition": condition_40MHz})
                        config_dict_40Mhz.update({"default_xtal_frequency_hz": default_xtal_frequency_hz})

                        config_dict_40Mhz.update(
                                {"phy_name_override_1": phy_1})
                        config_dict_40Mhz.update(
                                {"phy_name_override_2": phy_2})

                        if is_sub_giga:
                            config_dict_40Mhz.update(
                                {"channelName_1": "first_channelSubGHz"})
                            config_dict_40Mhz.update(
                                {"channelName_2": "second_channelSubGHz"})
                        else:
                            config_dict_40Mhz.update(
                                {"channelName_1": "channelSubGHz"})
                            config_dict_40Mhz.update(
                                {"channelName_2": "channel2p4"})

                        out_list.append(config_dict_40Mhz)

                    if has_38p4mhz_variant:
                        folder_384Mhz = folder + "/rf384mhz"
                        slcc_path_384Mhz = folder_384Mhz + "/" + "radio_settings.radioconf"
                        out_radio_conf_file_384MHz = Configs._generator_base_path.parent / slcc_path_384Mhz

                        condition_384MHz = list()
                        condition_384MHz.extend(condition)
                        condition_384MHz.append("hardware_board_has_hfxo_freq_38.4mhz")

                        config_dict_384Mhz = dict()

                        config_dict_384Mhz.update(
                            {"inputConfigFile": str(in_radio_conf_file)})
                        config_dict_384Mhz.update(
                            {"outputConfigFile": self.get_true_path(out_radio_conf_file_384MHz)})
                        config_dict_384Mhz.update({"slcc_path": slcc_path_384Mhz})
                        config_dict_384Mhz.update(
                            {"die": self.get_die_from_sdid(die_name)})
                        config_dict_384Mhz.update({"sdid": die_name})
                        config_dict_384Mhz.update({"profile": phy_profile})
                        config_dict_384Mhz.update({"phy": phy_1})
                        config_dict_384Mhz.update({"condition": condition_384MHz})
                        config_dict_384Mhz.update({"default_xtal_frequency_hz": default_xtal_frequency_hz})

                        config_dict_384Mhz.update(
                                {"phy_name_override_1": phy_1})
                        config_dict_384Mhz.update(
                                {"phy_name_override_2": phy_2})

                        if is_sub_giga:
                            config_dict_384Mhz.update(
                                {"channelName_1": "first_channelSubGHz"})
                            config_dict_384Mhz.update(
                                {"channelName_2": "second_channelSubGHz"})
                        else:
                            config_dict_384Mhz.update(
                                {"channelName_1": "channelSubGHz"})
                            config_dict_384Mhz.update(
                                {"channelName_2": "channel2p4"})

                        out_list.append(config_dict_384Mhz)

                    if has_39mhz_variant:
                        folder_39Mhz = folder + "/rf39mhz"
                        slcc_path_39Mhz = folder_39Mhz + "/" + "radio_settings.radioconf"
                        out_radio_conf_file_39MHz = Configs._generator_base_path.parent / slcc_path_39Mhz

                        condition_39MHz = list()
                        condition_39MHz.extend(condition)
                        condition_39MHz.append("hardware_board_has_hfxo_freq_39mhz")

                        config_dict_39MHz = dict()

                        config_dict_39MHz.update(
                            {"inputConfigFile": str(in_radio_conf_file)})
                        config_dict_39MHz.update(
                            {"outputConfigFile": self.get_true_path(out_radio_conf_file_39MHz)})
                        config_dict_39MHz.update({"slcc_path": slcc_path_39Mhz})
                        config_dict_39MHz.update(
                            {"die": self.get_die_from_sdid(die_name)})
                        config_dict_39MHz.update({"sdid": die_name})
                        config_dict_39MHz.update({"profile": phy_profile})
                        config_dict_39MHz.update({"phy": phy_1})
                        config_dict_39MHz.update({"condition": condition_39MHz})
                        config_dict_39MHz.update({"default_xtal_frequency_hz": default_xtal_frequency_hz})

                        config_dict_39MHz.update(
                                {"phy_name_override_1": phy_1})
                        config_dict_39MHz.update(
                                {"phy_name_override_2": phy_2})

                        if is_sub_giga:
                            config_dict_39MHz.update(
                                {"channelName_1": "first_channelSubGHz"})
                            config_dict_39MHz.update(
                                {"channelName_2": "second_channelSubGHz"})
                        else:
                            config_dict_39MHz.update(
                                {"channelName_1": "channelSubGHz"})
                            config_dict_39MHz.update(
                                {"channelName_2": "channel2p4"})

                        out_list.append(config_dict_39MHz)

        return sorted(out_list, key=lambda i: (i['sdid'], i['slcc_path']))

    def generate_custom_single_content(self, profile, die_dict, reduced_database):
        out_list = list()

        for die_name in die_dict.keys():
            if("device_generic_family_efr32xg21" in die_name):
                continue # skip panther as ther is no radio config
            if reduced_database.get(die_name) is None:
                continue
            if reduced_database.get(die_name).get(profile.split("_")[0]) is None:
                continue

            die_data = die_dict.get(die_name)
            default_bands = die_data.get("default_bands")
            supports_2p4GHz = die_data.get("supports_2p4GHz")
            supports_subGHz = die_data.get("supports_subGHz")

            supports_zwave = die_data.get("supports_zwave")
            if supports_zwave is None:
                supports_zwave = False

            if supports_subGHz:
                default_subgiga = get_default_sub_giga(default_bands)
                if "hardware_board_default_rf_band_2400" == default_subgiga:
                        supports_subGHz = False
            if supports_2p4GHz:
                default_24giga = "hardware_board_default_rf_band_2400"

            if supports_subGHz:
                phy = reduced_database.get(die_name).get(
                    profile.split("_")[0]).get(default_subgiga)
                if phy is None:
                    continue
                band = "rf_band_" + self.get_rf_band(default_subgiga)
                folder = get_folder_name(profile, self.get_rf_band(
                    default_subgiga), False) + "/" + die_name + "/" + band
                slcc_path = folder + "/" + "radio_settings.radioconf"

                condition = list()
                condition.append("device_has_radio_subghz")
                condition.append(die_name)
                unless = list()
                unless.append("hardware_board_rb")
                unless.append("hardware_board_tb")
                unless.append("hardware_board_stk")

                config_dict = dict()

                config_dict.update({"slcc_path": slcc_path})
                config_dict.update({"die": self.get_die_from_sdid(die_name)})
                config_dict.update({"sdid": die_name})
                config_dict.update({"condition": condition})
                config_dict.update({"unless": unless})
                config_dict.update({"supports_zwave": supports_zwave})

                out_list.append(config_dict)

            if supports_2p4GHz:
                phy = reduced_database.get(die_name).get(
                    profile.split("_")[0]).get(default_24giga)
                if phy is None:
                    continue
                band = "rf_band_" + self.get_rf_band(default_24giga)
                folder = get_folder_name(profile, self.get_rf_band(
                    default_24giga), False) + "/" + die_name + "/" + band
                slcc_path = folder + "/" + "radio_settings.radioconf"

                condition = list()
                condition.append("device_has_radio_2g4hz")
                condition.append(die_name)
                unless = list()
                unless.append("device_has_radio_subghz")
                unless.append("device_has_radio_dualband")
                unless.append("hardware_board_rb")
                unless.append("hardware_board_tb")
                unless.append("hardware_board_stk")

                config_dict = dict()

                config_dict.update({"slcc_path": slcc_path})
                config_dict.update({"die": self.get_die_from_sdid(die_name)})
                config_dict.update({"sdid": die_name})
                config_dict.update({"condition": condition})
                config_dict.update({"unless": unless})
                config_dict.update({"supports_zwave": supports_zwave})

                out_list.append(config_dict)

            if supports_subGHz and supports_2p4GHz:
                band = "rf_band_" + self.get_rf_band(default_subgiga)
                folder = get_folder_name(profile, self.get_rf_band(
                    default_subgiga), False) + "/" + die_name + "/" + band
                slcc_path = folder + "/" + "radio_settings.radioconf"

                condition = list()
                condition.append("device_has_radio_dualband")
                condition.append(die_name)
                unless = list()
                unless.append("hardware_board_rb")
                unless.append("hardware_board_tb")
                unless.append("hardware_board_stk")

                config_dict = dict()

                config_dict.update({"slcc_path": slcc_path})
                config_dict.update({"die": self.get_die_from_sdid(die_name)})
                config_dict.update({"sdid": die_name})
                config_dict.update({"condition": condition})
                config_dict.update({"unless": unless})
                config_dict.update({"supports_zwave": supports_zwave})

                out_list.append(config_dict)

        return sorted(out_list, key=lambda i: (i['sdid'], i['slcc_path']))

    def generate_custom_wmbus_content(self, die_dict, is_meter, reduced_database):
        out_list = list()

        profile = "Mbus"

        for die_name in die_dict.keys():
            if("device_generic_family_efr32xg21" in die_name):
                continue # skip panther as ther is no radio config
            die_data = die_dict.get(die_name)
            default_bands = die_data.get("default_bands")

            default_band = get_wmbus_default_phy(default_bands)

            if reduced_database.get(die_name) is None:
                continue
            if reduced_database.get(die_name).get(profile) is None:
                continue

            if default_band is not None:

                band = "rf_band_" + self.get_rf_band(default_band)
                folder = get_folder_name(profile, self.get_rf_band(
                    default_band), is_meter) + "/" + die_name + "/" + band
                slcc_path = folder + "/" + "radio_settings.radioconf"

                condition = list()
                condition.append("device_has_radio_subghz")
                condition.append(die_name)
                unless = list()
                unless.append("hardware_board_rb")
                unless.append("hardware_board_tb")
                unless.append("hardware_board_stk")

                config_dict = dict()

                config_dict.update({"slcc_path": slcc_path})
                config_dict.update({"die": self.get_die_from_sdid(die_name)})
                config_dict.update({"sdid": die_name})
                config_dict.update({"condition": condition})
                config_dict.update({"unless": unless})

                out_list.append(config_dict)
        return sorted(out_list, key=lambda i: (i['sdid'], i['slcc_path']))

    def generate_custom_multipy_content(self, die_dict):
        sort_list = sorted(self.multi_phy_slcc_path)
        out_list = list()

        for die_name in die_dict.keys():
            if("device_generic_family_efr32xg21" in die_name):
                continue
            die_data = die_dict.get(die_name)
            default_bands = die_data.get("default_bands")
            supports_2p4GHz = die_data.get("supports_2p4GHz")
            supports_subGHz = die_data.get("supports_subGHz")

            if supports_subGHz:
                default_subgiga = get_default_sub_giga(default_bands)
                if "hardware_board_default_rf_band_2400" == default_subgiga:
                    supports_subGHz = False
                else:
                    sub_gig_band = "rf_band_" + self.get_rf_band(default_subgiga)
            if supports_2p4GHz:
                default_24giga = "hardware_board_default_rf_band_2400"
                band_2p4GHz = "rf_band_" + self.get_rf_band(default_24giga)

            if supports_subGHz:
                slcc_path = None
                for path in sort_list:
                    if die_name in path:
                        if sub_gig_band in path:
                            if "subGHz" in path:
                                slcc_path = path
                                break
                if slcc_path is not None:
                    condition = list()
                    condition.append("device_has_radio_subghz")
                    condition.append(die_name)
                    unless = list()
                    unless.append("device_has_radio_2g4hz")
                    unless.append("device_has_radio_dualband")
                    unless.append("hardware_board_rb")
                    unless.append("hardware_board_tb")
                    unless.append("hardware_board_stk")

                    config_dict = dict()

                    config_dict.update({"slcc_path": slcc_path})
                    config_dict.update(
                        {"die": self.get_die_from_sdid(die_name)})
                    config_dict.update({"sdid": die_name})
                    config_dict.update({"condition": condition})
                    config_dict.update({"unless": unless})

                    out_list.append(config_dict)

            if supports_2p4GHz and supports_subGHz:
                slcc_path = None
                for path in sort_list:
                    if die_name in path:
                        if sub_gig_band in path:
                            if "2p4GHz" in path:
                                slcc_path = path
                                break
                if slcc_path is not None:
                    condition = list()
                    condition.append("device_has_radio_dualband")
                    condition.append(die_name)
                    unless = list()
                    unless.append("hardware_board_rb")
                    unless.append("hardware_board_tb")
                    unless.append("hardware_board_stk")

                    config_dict = dict()

                    config_dict.update({"slcc_path": slcc_path})
                    config_dict.update(
                        {"die": self.get_die_from_sdid(die_name)})
                    config_dict.update({"sdid": die_name})
                    config_dict.update({"condition": condition})
                    config_dict.update({"unless": unless})

                    out_list.append(config_dict)

        return sorted(out_list, key=lambda i: (i['sdid'], i['slcc_path']))
