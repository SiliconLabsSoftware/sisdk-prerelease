import os.path
from pathlib import Path

import ast
import yaml
import argparse
import xml.etree.ElementTree as ET
from hwmux_client.hwmux_api import HwMuxApi

example_config_name = "../../gsdk/btmesh_production_templates.xml"
bootloader_example_config_name = (
    "../../gsdk/platform/bootloader/bootloader_production_templates.xml"
)
diff = "../../gsdk/test_suite_ci.yaml"
output_file_name = "appsToBuild.yaml"
combined_test_file_path = "combined_tests.txt"

"""@Brief: This script generates appsToBuild.yaml file from btmesh_production_template.xml. Its purpose is to get the
current board list from the BTMESH testbed, iterate through the production template, and generate all supported
board-example and board-bootloader combinations. It also takes into account the output of diff.py script,
thus generating apps and SQA tests only for the application that changed in the PR.
This script also reads combined_tests.txt from the given SQA-pipeline repository.
This file describes which applications are tested in combination with one-and-other, so that if one of the slcps is changed
in the pull-request, the corresponding ones are built as well so the tests can run.
The output file is appsToBuild.yaml."""


class ExamplesToGenerate:
    def __init__(self, path, name, board_type):
        self.path = path
        self.name = name
        self.board_type = board_type

    def __eq__(self, other):
        if isinstance(other, ExamplesToGenerate):
            return (
                    self.path == other.path
                    and self.name == other.name
                    and self.board_type == other.board_type
            )
        return False


def generate_yaml_format(examples: list, output_file):
    with open(output_file, "w") as file:
        file.write("test_suites:\n")
        file.write(f"  - test_suite:\n")
        for example in examples:
            file.write(f"""    - test_case:\n""")
            file.write(f"""      app: "{example.path}"\n""")
            file.write(f"""      update_list:\n""")
            file.write(f"""        - update:\n""")
            file.write(f"""          name: {example.name}\n""")
            file.write(f"""          with: "{example.board_type}"\n""")


def dump_to_yaml(file_path, examples):
    generate_yaml_format(examples, file_path)


def get_board_list(mux, label):
    brd_list = list()
    print("Searching for..: " + label)
    group_ids = mux.labels_api.labels_list(name=label).results[0].device_groups
    groups = mux.groups_api.groups_list(id__in=group_ids).results
    for g in groups:
        for dev in g.devices:
            brd_name = str(dev.part.part_no).removesuffix(str(dev.part.revision))
            if brd_name not in brd_list:
                brd_list.append(brd_name)
    print(f"Board types found in testbed {label} are: ", brd_list)
    return brd_list


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--hwmux_label", type=str, required=True, help="The label of hwmux group to use"
    )
    parser.add_argument(
        "--hwmux_token", type=str, required=True, help="The token key for HWMUX"
    )
    args = parser.parse_args()
    return args


def generate_output(
        testbed_boards,
        supported_boards,
        project_path,
        slcp_name,
        diff_list,
        is_example=True,
):
    for tb_board in testbed_boards:
        if tb_board in supported_boards:
            # Drop .slcp after slcp name
            name = slcp_name + "_" + tb_board
            path = ""
            if is_example:
                path = "" + project_path
            else:
                path = "platform/bootloader/" + project_path
            bootloader_for_board = ExamplesToGenerate(
                path=path, name=name, board_type=tb_board
            )
            diff_list.append(bootloader_for_board)


def determine_affected_apps(slcps_affected_by_change):
    if os.path.exists(diff):
        with open(diff, "r", encoding="utf-8") as diff_py:
            changed_from_last_version = yaml.safe_load(diff_py)
            test_suites = changed_from_last_version["test_suites"][0]["test_suite"]
            for affected_slcp in test_suites:
                if affected_slcp["app"] not in slcps_affected_by_change:
                    slcps_affected_by_change.append(affected_slcp["app"])
    print("SLCPs affected by change:")
    print(*slcps_affected_by_change, sep="\n")


def crosscheck_production_template_with_changes(
        testbed_board_list,
        production_template_path,
        diff_list,
        input_diff_list,
        is_example=True,
        bootloaders_should_include=None,
):
    if bootloaders_should_include is None:
        bootloaders_should_include = []
    with open(production_template_path, "r", encoding="utf-8") as example_config:
        content = example_config.read()
        root = ET.fromstring(content)
        for example in root:
            example_elements = list(example)
            ex_slcp_name = example.attrib["name"]
            project_path = ""
            board_compatibility = ""
            for prop in example_elements:
                if prop.attrib["key"] == "projectFilePaths":
                    project_path = prop.attrib["value"]
                elif prop.attrib["key"] == "boardCompatibility":
                    board_compatibility = prop.attrib["value"]
            if (not any(project_path in s for s in input_diff_list)) and is_example:
                # There are no changes to this application in the current PR.
                continue
            if (not is_example) and not (
                    (bootloaders_should_include is not None)
                    and any(
                substring in ex_slcp_name
                for substring in bootloaders_should_include
            )
            ):
                continue
            if project_path == "" or board_compatibility == "":
                raise Exception(
                    "Couldn't find project path or board_compatibility attribute under "
                    + ex_slcp_name
                    + " attribute"
                )
            generate_output(
                testbed_boards=testbed_board_list,
                slcp_name=ex_slcp_name,
                supported_boards=board_compatibility,
                project_path=project_path,
                diff_list=diff_list,
                is_example=is_example,
            )
    if is_example:
        print("SLCPs supported on test boards and affected by change:")
    else:
        print("Bootloaders supported on test boards:")
    print()
    for gen in diff_list:
        print(gen.name)


def should_build_apps(path):
    if "freertos" in path:
        return False
    if "micriumos" in path:
        return False
    return True


def get_all_slcps_in_folder(folder_path):
    slcp_files = []
    # Iterate through the folder
    for root, dirs, files in os.walk("../../gsdk/" + folder_path):
        for file in files:
            # Check if the file has .slcp extension
            if file.endswith(".slcp"):
                # Get the full file path and add it to the list
                slcp_files.append(folder_path + "/" + file)
    return slcp_files


def get_additional_slcps_for_combined_tests(slcps_affected_by_change):
    try:
        additional_slcps = list()
        with open(combined_test_file_path, "r") as combined_test_file:
            for line in combined_test_file.readlines():
                path_list = ast.literal_eval(line)
                entry_affected = False
                for path in path_list:
                    for affected_slcp in slcps_affected_by_change:
                        # Check if any of these constraints apply to the changed slcp list
                        if not (path in affected_slcp):
                            continue
                        elif "btmesh_ncp_empty" not in path:
                            print(f"Found combined test for {affected_slcp}")
                            print(f"Therefore building:")
                            print(*path_list, sep='\n')
                            entry_affected = True
                if entry_affected:
                    for path in path_list:
                        if path.endswith(".slcp"):
                            if (path not in additional_slcps) and should_build_apps(path):
                                additional_slcps.append(path)
                        else:
                            slcps = get_all_slcps_in_folder(path)
                            for slcp in slcps:
                                if (slcp not in additional_slcps) and should_build_apps(slcp):
                                    additional_slcps.append(slcp)
        slcps_affected_by_change += additional_slcps
    except Exception as err:
        print(f"[ERROR] Couldn't get combinational constraints due to: {err} ")
        print("Make sure that combined_tests.txt is uploaded to the correct SQA PIPELINE branch.")


def main():
    args = parse_args()
    hw_mux_token = args.hwmux_token
    hw_mux_label = args.hwmux_label
    mux_handler: HwMuxApi = HwMuxApi(user_token=hw_mux_token)
    testbed_board_list = get_board_list(mux=mux_handler, label=hw_mux_label)
    testbed_board_list = [element.lower() for element in testbed_board_list]
    # Determine changes by commit
    slcps_affected_by_change = list()
    determine_affected_apps(slcps_affected_by_change)
    get_additional_slcps_for_combined_tests(slcps_affected_by_change)
    if len(slcps_affected_by_change) == 0:
        print("No changes, no tests should run.")
        with open(output_file_name, "w") as file:
            file.write(
                "#No tests should run as there are no changes to any of the apps"
            )

    # Determine which examples are affected
    example_classes = list()
    crosscheck_production_template_with_changes(
        testbed_board_list,
        example_config_name,
        example_classes,
        slcps_affected_by_change,
        is_example=True,
    )

    # Determine which bootloaders may be needed to run those applications
    # For now, generate all bootloaders only with "single", "spi" or "uart" in their names
    bootloader_classes = list()
    if not len(example_classes) == 0:
        bootloaders_should_include = ["single", "spi", "uart"]
        crosscheck_production_template_with_changes(
            testbed_board_list,
            bootloader_example_config_name,
            bootloader_classes,
            slcps_affected_by_change,
            is_example=False,
            bootloaders_should_include=bootloaders_should_include,
        )
    generated_classes = example_classes + bootloader_classes
    dump_to_yaml(output_file_name, generated_classes)

    print(f"Generated to: {os.path.abspath(output_file_name)}")
    print(f"Apps generated: {len(example_classes)}")
    print(f"Bootloaders generated: {len(bootloader_classes)}")


if __name__ == "__main__":
    main()
