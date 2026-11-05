#!/usr/bin/env python3

import os
import platform
import re
import sys
from argparser import Argparser

# Supported python versions.
supportedPythonVersion = {
    "majorVersion": 3,
    "minorVersion": 6, # or more
    "patchVersion": 0  # or more
  }

unicolorPlatformList = ["linux"]

class Colors(object):
  HEADER = '\033[95m'
  OKBLUE = '\033[94m'
  OKGREEN = '\033[92m'
  WARNING = '\033[93m'
  FAIL = '\033[91m'
  ENDC = '\033[0m'
  BOLD = '\033[1m'
  UNDERLINE = '\033[4m'

class Engine(Argparser):
  _infoSeparator = '####################################################################################################'
  _errorSeparator = '----------------------------------------------------------------------------------------------------'
  _successSeparator = '----------------------------------------------------------------------------------------------------'
  _platform = re.sub("cygwin.*", "cygwin", platform.system().lower())
  
  def __init__(self):
    Argparser.__init__(self)

  def printCmdSuccess(self, text):
    self.printSuccess(self._successSeparator)
    self.printSuccess(text)
    self.printSuccess(self._successSeparator)

  def printCmdInfo(self, text):
    print(self._infoSeparator)
    print(text)
    print(self._infoSeparator)

  def printCmdVerbose(self, text):
    if self._verbose:
      self.printInfoVerbose(self._infoSeparator)
      self.printInfoVerbose(text)
      self.printInfoVerbose(self._infoSeparator)

  def printCmdError(self, text):
    self.printError(self._errorSeparator)
    self.printError('Error:')
    self.printError(text)
    self.printError(self._errorSeparator)
    exit(1)

  def printInfoVerbose(self, text):
    if self._verbose:
      if self._platform in unicolorPlatformList:
        print(text)
      else:
        print(Colors.OKBLUE + text + Colors.ENDC)

  def printWarning(self, text):
    if self._platform in unicolorPlatformList:
      print(text)
    else:
      print(Colors.WARNING + text + Colors.ENDC)

  def printError(self, text):
    if self._platform in unicolorPlatformList:
      print(text)
    else:
      print(Colors.FAIL + text + Colors.ENDC)

  def printSuccess(self, text):
    if self._platform in unicolorPlatformList:
      print(text)
    else:
      print(Colors.OKGREEN + text + Colors.ENDC)

  # Check python version
  def checkPythonVersion(self):
    isSupportedPython = False
    if sys.version_info[0] == supportedPythonVersion["majorVersion"] and \
       sys.version_info[1] >= supportedPythonVersion["minorVersion"] and \
       sys.version_info[2] >= supportedPythonVersion["patchVersion"]:
      isSupportedPython = True
    if not isSupportedPython:
      errorMsg = "Invalid python version: %d.%d.%d" %\
                  (sys.version_info[0], sys.version_info[1], sys.version_info[2]) +\
                 "\nSupported python version: %d.%d+.%d+" %\
                  (supportedPythonVersion["majorVersion"],\
                  supportedPythonVersion["minorVersion"],\
                  supportedPythonVersion["patchVersion"])
      self.printCmdError(errorMsg)


# Test what the engine gives
def main():
  engine = Engine()
  engine.printInfoVerbose('Info Message')
  engine.printWarning('Warning Message')
  engine.printError('Error Message')
  engine.printSuccess('Success Message')
  engine.printCmdVerbose('cmd Info Message')
  engine.printCmdError('cmd Error Message with exit(1)')


if __name__ == "__main__":
  main()
