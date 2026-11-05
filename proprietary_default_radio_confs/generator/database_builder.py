#!/usr/bin/env python3

import pathlib
import re
import yaml

import pprint

from engine import Engine
from util import Util
from hw_database_builder import HwDatabaseBuilder
from xml_database_builder import XMLDatabaseBuilder
from radioconf_database_builder import RadioConfDatabaseBuilder
from phy_restriction import PhyRestriction


class DatabaseBuilder(HwDatabaseBuilder, XMLDatabaseBuilder, RadioConfDatabaseBuilder, PhyRestriction, Engine):
    hw_db = dict()
    profiles_by_family = dict()
    radio_dict = dict()
    phy_rest = dict()

    reduced_database = dict()

    def build_databases(self):
        self.hw_db = self.build_hw_database()
        self.profiles_by_family = self.build_profiles_database()
        self.radio_dict = self.build_radio_conf_database()
        self.phy_rest = self.read_restrictions()
        self.reduced_database = self.combine_die_and_xml_with_restriction()

    def combine_die_and_xml_with_restriction(self):
        comb_database = dict()
        self.printCmdVerbose("Combining databases")

        for hw_family_id, hw_family_data in self.hw_db.items():
            die_dict = dict()
            default_bands = hw_family_data.get("default_bands")

            list_of_profiles = self.phy_rest.keys()

            for profile in list_of_profiles:
                profile_dict = dict()
                for default_band in default_bands:
                    default_band_phy_name = self.get_default_bands_phy(
                        hw_family_id, profile, default_band)
                    if default_band_phy_name is not None:
                        profile_dict.update(
                            {default_band: default_band_phy_name})
                if len(profile_dict) > 0:
                    die_dict.update({profile: profile_dict})
            if len(die_dict) > 0:
                comb_database.update({hw_family_id: die_dict})

        if len(comb_database) > 0:
            self.printCmdSuccess("Combining databases finished")
        else:
            self.printCmdError("Combining databases finished")
        return comb_database

    def get_phys_for_rf_band(self, base_phys_from_xml, int_rf_band, offset):
        selected_dict = dict()
        max_freq = int_rf_band + offset
        min_freq = int_rf_band - offset
        for phy_name in base_phys_from_xml.keys():
            phy = base_phys_from_xml.get(phy_name)
            base_freq = phy.get("base_frequency_hz")
            int_base_freq = int(base_freq)
            if int_base_freq < max_freq and int_base_freq > min_freq:
                selected_dict.update({phy_name: phy})
        return selected_dict

    def get_phys_with_modulation(self, band_phys, modulation_rest):
        if modulation_rest is None:
            return band_phys
        selected_dict = dict()
        for phy_name in band_phys.keys():
            phy = band_phys.get(phy_name)
            modulation = phy.get("modulation_type")
            if modulation == modulation_rest:
                selected_dict.update({phy_name: phy})
        return selected_dict

    def get_phys_with_datarate(self, band_phys, min_datarate, max_datarate):
        if min_datarate is None or max_datarate is None:
            return band_phys
        selected_dict = dict()
        for phy_name in band_phys.keys():
            phy = band_phys.get(phy_name)
            bitrate = int(phy.get("bitrate"))
            if bitrate <= max_datarate and bitrate >= min_datarate:
                selected_dict.update({phy_name: phy})
        return selected_dict

    def get_fastes_phy(self, band_phys):
        fastest_phy_name = ""
        fastest_phy_bitrate = 0
        for phy_name in band_phys.keys():
            phy = band_phys.get(phy_name)
            bitrate = int(phy.get("bitrate"))
            if bitrate > fastest_phy_bitrate:
                fastest_phy_bitrate = bitrate
                fastest_phy_name = phy_name
        return fastest_phy_name

    def get_default_bands_phy(self, device_id, profile, default_band):
        if self.profiles_by_family.get(device_id) is None:
            print(f"{device_id} doesn't have a xml file\n")
            return None
        profile_phys = self.profiles_by_family.get(device_id).get(profile)
        if profile_phys is None:
            return None

        if profile == "Mbus":
            return "Mbus phy is in radioconf file"

        up_offset = 100000000
        rf_band = int(self.get_rf_band(default_band)) * 1000000

        if rf_band > 1000000000:
            offset = up_offset
            modulation_rest = self.phy_rest.get(
                profile).get("2p4GHz").get("codind_prority")
            min_datarate = int(self.phy_rest.get(
                profile).get("2p4GHz").get("min_datarate"))
            max_datarate = int(self.phy_rest.get(
                profile).get("2p4GHz").get("max_datarate"))
        elif 1000000000 > rf_band > 700000000:
            offset = 20000000
            modulation_rest = self.phy_rest.get(
                profile).get("subGHz").get("codind_prority")
            min_datarate = int(self.phy_rest.get(
                profile).get("subGHz").get("min_datarate"))
            max_datarate = int(self.phy_rest.get(
                profile).get("subGHz").get("max_datarate"))
        else:
            offset = 30000000
            modulation_rest = self.phy_rest.get(
                profile).get("subGHz").get("codind_prority")
            min_datarate = int(self.phy_rest.get(
                profile).get("subGHz").get("min_datarate"))
            max_datarate = int(self.phy_rest.get(
                profile).get("subGHz").get("max_datarate"))

        band_phys = self.get_phys_for_rf_band(profile_phys, rf_band, offset)
        if len(band_phys) == 0:
            band_phys = self.get_phys_for_rf_band(
                profile_phys, rf_band, offset*3)
            if len(band_phys) == 0:
                return None

        band_phy_with_datarate = self.get_phys_with_datarate(
            band_phys, min_datarate, max_datarate)
        if len(band_phy_with_datarate) == 0:
            band_phy_with_datarate = band_phys

        band_phy_with_modulation = self.get_phys_with_modulation(
            band_phy_with_datarate, modulation_rest)
        if len(band_phy_with_modulation) == 0:
            band_phy_with_modulation = band_phy_with_datarate

        fastest_phy_name = self.get_fastes_phy(band_phy_with_modulation)

        return fastest_phy_name
