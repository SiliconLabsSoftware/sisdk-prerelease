import sys
import os
from setuptools import setup, find_packages


execfile('pyradioconfig\_version.py')
print(__version__)

setup (
    name = "pyradioconfig",
    version = __version__,
    description="Radio Configurator.",
    long_description="""\
Radio Configurator""",
    author="Silicon Labs",
    author_email="", # Removed to limit spam harvesting.
    url="http://silabs.com",
    packages = find_packages(exclude="unit_tests"),


    download_url = "",
    zip_safe = False
)