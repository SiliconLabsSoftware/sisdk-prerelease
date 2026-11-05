#!/usr/bin/env python3

import os
import pathlib
import re
import sys
from configurations import Configs

class Util(object):

    def get_codename_from_family_name(self, die):
        for key in Configs._device_family_names.keys():
            if Configs._device_family_names.get(key) == die:
                return key
        return None

    def get_die_from_sdid(self, sdid):
        return Configs._device_family_names.get(sdid)

    def get_rf_band(self, rf_band_string):
        m = re.match(r'[a-zA-Z_]+([0-9]+)', rf_band_string)
        rf_band = m.group(1)
        return rf_band

    def generate_platform_dependent_path(self, path):
        path_string = str(path.absolute())
        if os.name == 'nt':
            # If the platform is Windows, return the path as a WindowsPath object
            return pathlib.PureWindowsPath(path_string)
        elif os.name == 'posix' and 'CYGWIN' in os.uname()[0]:
            # If the platform is Cygwin, convert the path to a Windows path
            windows_path = pathlib.Path(os.path.abspath(path_string)).as_posix().replace('/', '\\')
            if windows_path.startswith('\\cygdrive\\'):
                # Remove the "cygdrive" prefix from the Windows path
                windows_path = windows_path[10].upper() + ':' + windows_path[11:]
            return pathlib.PureWindowsPath(windows_path)
        else:
            # If the platform is neither Windows nor Cygwin, return the path as a PosixPath object
            return pathlib.PosixPath(path_string)



    def get_path_for_radioconf(self, radiconf_dict, project, device_sdid):
        project_dict = radiconf_dict.get(project)
        if project_dict is None:
            return None
        path = project_dict.get(device_sdid)
        if path is None:
            path = project_dict.get("default")
        return str(self.generate_platform_dependent_path(path))

    def get_true_path(self, path):
        path = pathlib.Path(path)
        return str(self.generate_platform_dependent_path(path))

    # Print iterations progress

    def printProgressBar(self, iteration, total, prefix='', suffix='', decimals=1, length=100, fill='█', printEnd="\r"):
        """
        Call in a loop to create terminal progress bar
        @params:
            iteration   - Required  : current iteration (Int)
            total       - Required  : total iterations (Int)
            prefix      - Optional  : prefix string (Str)
            suffix      - Optional  : suffix string (Str)
            decimals    - Optional  : positive number of decimals in percent complete (Int)
            length      - Optional  : character length of bar (Int)
            fill        - Optional  : bar fill character (Str)
            printEnd    - Optional  : end character (e.g. "\r", "\r\n") (Str)
        """
        percent = ("{0:." + str(decimals) + "f}").format(100 *
                                                         (iteration / float(total)))
        filledLength = int(length * iteration // total)
        bar = fill * filledLength + '-' * (length - filledLength)
        print(f'\r{prefix} |{bar}| {percent}% {suffix}', end=printEnd)
        # Print New Line on Complete
        if iteration == total:
            print()
