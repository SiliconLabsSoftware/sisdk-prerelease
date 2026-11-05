#!/usr/bin/env python3

from pathlib import Path
import yaml
import subprocess

from engine import Engine
from util import Util
from configurations import Configs


class HwDatabaseBuilder(Util, Engine):

    def build_hw_database(self):
        self.printCmdInfo("Building HW database")
        family_database = self.create_device_family_database()
        if len(family_database) > 0:
            self.printCmdSuccess("Building HW database finished")
        else:
            self.printCmdError("Building HW database finished")
        return family_database


    def create_device_database(self) -> (dict):
        """
        Creates a database for all devices with the relevant attributes.

        Example:
        `{"efr32fg25a121f1024im48": {
            "device_family": "device_generic_family_efr32xg25",
            "has_radio_subGhz": True, 
            "has_radio_2p4Ghz": True, 
            "supports_zwave": False
            }}`.
        """
        component_path = Path(Configs._project_rel_paths["devices"])
        device_dirs = [
            self._resolve_component_path("platform_core", component_path),
            self._resolve_component_path("platform_core_internal", component_path),
        ]
        slcc_suffix = Configs._suffixes["component"]
        slcc_files = [
            f for d in device_dirs for f in d.iterdir()
            if f.is_file() and f.suffix == slcc_suffix
        ]
        count_of_files = len(slcc_files)

        print('----------------------------------------------------------------------------------------------------')
        print("Building Part database")
        device_db = dict()

        files_read = 0
        for file in slcc_files:
            # check if file is a valid component file
            if not file.is_file():
                continue
            if file.suffix != Configs._suffixes["component"]:
                continue

            with open(file, 'r') as yaml_file:
                device_comp_content = yaml.load(yaml_file, Loader=yaml.SafeLoader)

            # convert data structure if '!!omap' is missing form top of component file
            if isinstance(device_comp_content, dict):
                device_comp_content = list(device_comp_content.items())

            # parse device component file for the following:
            device = file.stem
            device_family = None
            device_sdid = None          # some files are missing the generic_family identifier and have the legacy SDID instead
            has_radio_2p4ghz = False
            has_radio_subghz = False
            supports_zwave = False
            skip_file = False

            for section_name, section_content in device_comp_content: 
                if section_name == "provides":
                    for provide in section_content:
                        provide_value = provide.get("name")
                        if "device_generic_family_" in provide_value:
                            device_family = provide_value
                        if "device_sdid_" in provide_value:
                            device_sdid = provide_value
                        if "device_has_radio_2g4hz" in provide_value:
                            has_radio_2p4ghz = True
                        if "device_has_radio_subghz" in provide_value:
                            has_radio_subghz = True
                        if "device_supports_zwave" in provide_value:
                            supports_zwave = True
                        # TODO: error handling

            # workaround for legacy files missing the generic_family_ provide
            if device_family is None:
                match device_sdid:
                    case "device_sdid_205":
                        device_family = "device_generic_family_efr32xg22"
                    case "device_sdid_210":
                        device_family = "device_generic_family_efr32xg23"
                    case "device_sdid_225":
                        device_family = "device_generic_family_efr32xg26"
                    case "device_sdid_235":
                        device_family = "device_generic_family_efr32xg28"
                    case _:
                        # not a valid device component file
                        skip_file = True
            
            files_read = files_read + 1
            self.printProgressBar(
                files_read, count_of_files, prefix='Progress:', suffix='Complete', length=50)
            
            if skip_file:
                continue
                
            device_db[device] = {
                "device_family": device_family,
                "has_radio_subghz": has_radio_subghz,
                "has_radio_2p4ghz": has_radio_2p4ghz,
                "supports_zwave": supports_zwave
            }
                
        return device_db


    def create_device_family_database(self):
        """
        Create a family database by aggregating board capabilities within each family.

        For each family, parameters are combined across all boards so that a parameter
        is set if any board in the family supports it.
        """
        device_db = self.create_device_database()
        family_db = dict()

        # get the path of the downloaded package
        component_path = Path(Configs._project_rel_paths["boards"])
        board_dirs = [
            self._resolve_component_path("boards", component_path),
            self._resolve_component_path("boards_internal", component_path),
        ]
        slcc_suffix = Configs._suffixes["component"]
        slcc_files = [
            f for d in board_dirs for f in d.iterdir()
            if f.is_file() and f.suffix == slcc_suffix
        ]
        count_of_files = len(slcc_files)

        print('----------------------------------------------------------------------------------------------------')
        print("Building Board database")
        files_read = 0

        for file in slcc_files:
            # check if file is a valid component file
            if not file.is_file() or file.suffix != Configs._suffixes["component"]:
                continue

            with open(file, 'r') as yaml_file:
                board_comp_content = yaml.load(yaml_file, Loader=yaml.SafeLoader)

            # convert data structure if '!!omap' is missing form top of component file
            if isinstance(board_comp_content, dict):
                board_comp_content = list(board_comp_content.items())

            board_id = file.stem
            device_id = None
            family = None
            default_rf_band = None
            supported_rf_bands = list()
            multiphy_combination = None
            supports_2p4GHz = False
            supports_subGHz = False
            supports_zwave = False
            has_40mhz_variant = False
            has_38p4mhz_variant = False
            has_39mhz_variant = False
            skip_file = False

            # parse board component file
            for section_name, section_content in board_comp_content:
                if section_name == "requires":
                    for require in section_content:
                        # check that the board contains a valid device
                        if require.get("name") in device_db:
                            device_id = require.get("name")
                            family = device_db[device_id].get("device_family")
                            supports_zwave = device_db[device_id].get("supports_zwave")
                    # no valid device found, skip this file
                    if device_id is None or family is None:
                        skip_file = True
                        break

                if section_name == "provides":
                    for provide in section_content:
                        provide_value = provide.get("name")
                        if provide_value == "hardware_board_has_hfxo_freq_40mhz":
                            has_40mhz_variant = True
                        if provide_value == "hardware_board_has_hfxo_freq_38.4mhz":
                            has_38p4mhz_variant = True
                        if provide_value == "hardware_board_has_hfxo_freq_39mhz":
                            has_39mhz_variant = True
                        if provide_value == "hardware_board_supports_rf_band_2400":
                            supports_2p4GHz = True
                        if "hardware_board_supports_rf_band_" in provide_value:
                            if int(self.get_rf_band(provide_value)) >= 1000:
                                supported_rf_bands.append(provide_value)
                            else:
                                # xg21 and xg22 parts don't have integrated subgig radio support - modules can have a third-party
                                # chip integrated which support it, but that is not configurable via radio configurator
                                if family != "device_generic_family_efr32xg21" and family != "device_generic_family_efr32xg22":
                                    supports_subGHz = True
                                    supported_rf_bands.append(provide_value)
                        if "hardware_board_default_rf_band_" in provide_value:
                            # 169MHz boards are internal and not used in the field
                            if provide_value != "hardware_board_default_rf_band_169":
                                default_rf_band = provide_value

                    # some board component files are missing the RF band provides
                    if default_rf_band is None:
                        skip_file = True
                        break

                    if len(supported_rf_bands) > 1:
                        combination = self._generate_multiphy_combination(default_rf_band, supported_rf_bands)
                        if default_rf_band != "hardware_board_default_rf_band_2400":
                            multiphy_combination = combination

            # file parsing complete
            files_read = files_read + 1
            self.printProgressBar(files_read, count_of_files, prefix='Progress:', suffix='Complete', length=50)

            # skip file if any error happened during parsing
            if skip_file:
                continue
            
            # no issues, update family database with board data
            family_entry = family_db.get(family)

            if family_entry is None:
                # initialize
                family_db[family] = {
                    "has_40mhz_variant": has_40mhz_variant,
                    "has_38p4mhz_variant": has_38p4mhz_variant,
                    "has_39mhz_variant": has_39mhz_variant,
                    "supports_subGHz": supports_subGHz,
                    "supports_2p4GHz": supports_2p4GHz,
                    "supports_zwave": supports_zwave,
                    "default_bands": {default_rf_band},
                    "multiphy_combinations": {multiphy_combination} if multiphy_combination else set()
                }
            else:
                # OR aggregation
                family_entry["has_40mhz_variant"] |= has_40mhz_variant
                family_entry["has_38p4mhz_variant"] |= has_38p4mhz_variant
                family_entry["has_39mhz_variant"] |= has_39mhz_variant
                family_entry["supports_subGHz"] |= supports_subGHz
                family_entry["supports_2p4GHz"] |= supports_2p4GHz
                family_entry["supports_zwave"] |= supports_zwave
                family_entry["default_bands"].add(default_rf_band)
                if multiphy_combination:
                    family_entry["multiphy_combinations"].add(multiphy_combination)
            
            # if 2.4 GHz is supported then add it to the defaults (legacy reasons - to keep old output)
            if family_db[family]["supports_2p4GHz"]:
                family_db[family]["default_bands"].add("hardware_board_default_rf_band_2400")

        return family_db

    # ------------------------------------- helper functions ------------------------------------

    def _generate_multiphy_combination(self, default_band, supported_bands):
        if "hardware_board_supports_rf_band_2400" in supported_bands and "hardware_board_default_rf_band_2400" != default_band:
            return default_band + ":" + "hardware_board_supports_rf_band_2400"
        else:
            for supported_band in supported_bands:
                default = self.get_rf_band(default_band)
                supported = self.get_rf_band(supported_band)
                if default in ["914", "915", "924"] and supported in ["914", "915", "924"]:
                    # don't allow combinations within this group
                    continue
                elif default == supported:
                    # combination must include two unique bands
                    continue
                else:
                    return default_band + ":" + supported_band

    def _resolve_component_path(self, slt_package: str, component_rel_path: Path) -> Path:
        command = ['slt', 'where', slt_package]
        self.printInfoVerbose(command)
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if result.returncode != 0:
            self.printError(f"Error running command {command}: {result.stderr.decode('utf-8')}")
            exit(1)
        self.printInfoVerbose(f"Output: {result.stdout.decode('utf-8')}")
        package_path = result.stdout.decode('utf-8').strip()
        full_path = Path(package_path) / component_rel_path
        self.printInfoVerbose(full_path)
        return full_path
            
