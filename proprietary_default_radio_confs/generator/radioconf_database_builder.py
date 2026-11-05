#!/usr/bin/env python3

from pathlib import Path
import re
import yaml
import xml.etree.ElementTree as ET
from util import Util
from engine import Engine
from configurations import Configs


class RadioConfDatabaseBuilder(Util, Engine):


    def build_radio_conf_database(self):
        radio_conf_dict = dict()

        full_path = Configs._generator_base_path / Configs._project_rel_paths["radio_configs"]

        count_of_files = len(list(full_path.glob('*'+Configs._suffixes["radio_config"])))

        self.printCmdInfo("Building radiconf database")
        self.printProgressBar(
            0, count_of_files, prefix='Progress:', suffix='Complete', length=50)

        files_read = 0

        for files in full_path.iterdir():
            if files.is_file():
                if files.suffix == Configs._suffixes.get("radio_config"):
                    if "__" in files.stem:
                        m = re.match(r'(\w+)__(device_generic_family_[a-z0-9]*)', files.stem)
                        if m is not None:
                            file_name = m.group(1).strip()
                            die = m.group(2).strip()
                            if die in Configs._device_family_names.keys():
                                org = radio_conf_dict.get(file_name)
                                if org is None:
                                    org = dict()
                                org.update({die: files})
                                radio_conf_dict.update({file_name: org})
                    else:
                        file_name = files.stem
                        org = radio_conf_dict.get(file_name)
                        if org is None:
                            org = dict()
                        org.update({"default": files})
                        radio_conf_dict.update({file_name: org})

                    files_read = files_read + 1
                    self.printProgressBar(
                        files_read, count_of_files, prefix='Progress:', suffix='Complete', length=50)

        if len(radio_conf_dict) > 0:
            self.printCmdSuccess("Building radiconf database finished")
        else:
            self.printCmdError("Building radiconf database finished")
        return radio_conf_dict
