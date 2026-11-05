#!/usr/bin/env python3

import pathlib
import yaml
import re
from util import Util
from engine import Engine
from configurations import Configs


class PhyRestriction(Util, Engine):

    def read_restrictions(self):
        self.printCmdInfo("Building Restriction database")

        full_path = Configs._generator_base_path / Configs._project_rel_paths["restriction_file"]

        with open(full_path, 'r') as yaml_file:
            restriction_content = yaml.load(yaml_file, Loader=yaml.SafeLoader)

        if restriction_content is not None and len(restriction_content) > 0:
            self.printCmdSuccess("Building Restriction database finished")
        else:
            self.printCmdError("Building Restriction database finished")
        return restriction_content
