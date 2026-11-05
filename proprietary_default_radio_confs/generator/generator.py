#!/usr/bin/env python3

import os
import platform
import pathlib
import re
import shutil
import tempfile
import sys
import pprint
import subprocess
from argparser import Argparser
from slcc_core import SlccCore

from database_builder import DatabaseBuilder
from output_content_generator import OutputContentGenerator
from json_file_creater import JSONFileCreater
from configurations import Configs
from util import Util
from pathlib import Path
from engine import Engine



class Generator(DatabaseBuilder, OutputContentGenerator, JSONFileCreater, Engine):
    def __init__(self):
        Argparser.__init__(self)

def main():
    generator = Generator()

    # save absolute path of generator base dir
    Configs._generator_base_path = Path(__file__).resolve().parent

    if generator._xml_update:
        generator.printSuccess(f"Running xml update on branch: {generator._xml_update}")
        repo_url = 'https://github.com/SiliconLabsInternal/wt-radio-config-generator.git'
        branch = generator._xml_update
        target_folder = Configs._generator_base_path / 'tmp_xml_update'
        shutil.rmtree(target_folder, ignore_errors=True)
        command = ['git', 'clone', '--no-checkout', '--depth=1', '--filter=blob:none', '-b', branch, repo_url, target_folder]
        generator.printInfoVerbose(command)
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if result.returncode != 0:
            generator.printError('Error cloning repository:')
            generator.printError(result.stderr.decode('utf-8'))
            shutil.rmtree(target_folder, ignore_errors=True)
        else:
            command = ['git', 'sparse-checkout', 'set', '--no-cone', 'node-generator/xmls']
            generator.printInfoVerbose(command)
            result = subprocess.run(command, cwd=target_folder, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            command = ['git', 'checkout']
            generator.printInfoVerbose(command)
            result = subprocess.run(command, cwd=target_folder, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            generator.printSuccess('Repository cloned successfully.')

            # Create a Path object for the current directory
            xmls_dir = target_folder / "node-generator/xmls"
            # Get a list of all subdirectories in the current directory
            subdirs = [d.name for d in xmls_dir.iterdir() if d.is_dir()]
            
            # Sort version numbers properly (semantic versioning)
            def version_key(version_str):
                """Convert version string to tuple for proper sorting"""
                try:
                    # Split by dots and convert each part to integer
                    parts = version_str.split('.')
                    return tuple(int(part) for part in parts)
                except ValueError:
                    # If conversion fails, treat as string
                    return (0, 0, 0)
            
            subdirs.sort(key=version_key)
            latest_subdir = subdirs[-1]

            generator.printInfoVerbose(f"Latest subdirectory: {str(latest_subdir)}")

            needed_version_subdir = latest_subdir
            if generator._xml_version:
                if generator._xml_version.lower() == "latest":
                    pass
                else:
                    if generator._xml_version in subdirs:
                        needed_version_subdir = generator._xml_version
                    else:
                        generator.printWarning('Wrong version number using latest.')

            src_path = target_folder / "node-generator/xmls" / needed_version_subdir
            dest_path = Configs._generator_base_path /"efr32_radio_configurator_api_factory/xml"
            # copy all files and subdirectories
            for item in os.listdir(src_path):
                source_path = os.path.join(src_path, item)
                destination_path = os.path.join(dest_path, item)
                shutil.copy2(source_path, destination_path)
            generator.printSuccess('Files overwritten.')
            shutil.rmtree(target_folder, ignore_errors=True)

    generator.printCmdInfo("Installing package dependencies")
    
    cmd = ["slt", "install", "-f", str(Configs._generator_base_path / "pkg.slt")]
    generator.printInfoVerbose(cmd)
    result = subprocess.run(cmd)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed ({result.returncode}):\n{result.stderr}")
    else:
        generator.printSuccess("Packages installed successfully.")

    generator.build_databases()

    projects_dict = dict()
    for project in Configs._project_types.keys():
        if project == "Mbus":
            project_list_meter = generator.generate_board_out_content(
                project, generator.hw_db, generator.reduced_database, generator.radio_dict, is_meter=True)
            project_list_collector = generator.generate_board_out_content(
                project, generator.hw_db, generator.reduced_database, generator.radio_dict, is_meter=False)
            projects_dict.update({Configs._project_types[project][True]: project_list_meter})
            projects_dict.update({Configs._project_types[project][False]: project_list_collector})
        else:
            project_list = generator.generate_board_out_content(
                project, generator.hw_db, generator.reduced_database, generator.radio_dict, rest_dict=generator.phy_rest)
            projects_dict.update({Configs._project_types[project]: project_list})

    full_list = list()
    for project in projects_dict.keys():
        full_list.extend(projects_dict[project])

    generator.create_json_file(full_list)

    custom_project_dict = dict()
    for project in Configs._project_types.keys():
        if project == "Mbus":
            project_list_meter = generator.generate_custom_out_content(
                project, generator.hw_db, generator.reduced_database, is_meter=True)
            project_list_collector = generator.generate_custom_out_content(
                project, generator.hw_db, generator.reduced_database, is_meter=False)
            custom_project_dict.update({Configs._project_types[project][True]: project_list_meter})
            custom_project_dict.update({Configs._project_types[project][False]: project_list_collector})
        else:
            project_list = generator.generate_custom_out_content(
                project, generator.hw_db, generator.reduced_database, rest_dict=generator.phy_rest)
            custom_project_dict.update({Configs._project_types[project]: project_list})

    slcc_generator = SlccCore()

    for project in projects_dict.keys():
        complete_list = list()
        generator.printInfoVerbose(f"Generate slcc for project {project}")
        complete_list.extend(custom_project_dict[project])
        complete_list.extend(projects_dict[project])
        slcc_generator.create_file_from(project, complete_list)
        generator.printSuccess(f"Generated slcc for project {project}")

    if generator._manipulator:
        os.environ['NPM_CONFIG_USERCONFIG'] = f"{Configs._generator_base_path}"
        # manipulator script is expecting the json path to be relative to cwd
        json_path_rel = Configs._file_names["radiconf_studio_json"]

        cmd = [
        "npx", "--yes",
        "-p", "@silabs-wasp/wasp-radio-xmls@master",
        "radio-manipulator",
        "-j", str(json_path_rel),
        "-f",
        "--sdk", str(needed_version_subdir)
        ]
        generator.printInfoVerbose(f"Running command: {' '.join(cmd)}")

        result = subprocess.run(
            cmd,
            cwd=Configs._generator_base_path,
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            raise Exception(
                f"Error running command ({result.returncode}):\n{result.stderr}"
            )
        generator.printInfoVerbose(f"Output: {result.stdout}")
        generator.printSuccess("Generated radioconfigs")


if __name__ == "__main__":
    main()
