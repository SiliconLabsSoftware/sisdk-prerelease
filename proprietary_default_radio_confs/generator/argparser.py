#!/usr/bin/env python3

import argparse
import sys

class Argparser(object):
  def __init__(self):
    self._args = self.parseCmdArguments()
    self._verbose = self._args.verbose
    self._manipulator = self._args.manipulator
    self._xml_update = self._args.xml_update
    self._xml_version = self._args.xml_version

  def parseCmdArguments(self):
    parser = argparse.ArgumentParser(
    description="Radio Configuration Generator")
    parser.add_argument(
      "--verbose",
      action='store_true',
      help="Verbose printing.")
    parser.add_argument(
      "--manipulator",
      action='store_true',
      help="Run Manupulator command as well.")
    parser.add_argument(
      "--xml_update",
      type=str,
      help="Copy XML files from WASP repo command as well, param is which branch")
    parser.add_argument(
      "--xml_version",
      type=str,
      default="latest",
      help="Copy XML files from WASP repo command as well, param is which version")
    return parser.parse_args()