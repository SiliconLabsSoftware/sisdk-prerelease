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
output_file_name = "appsToBuild.yaml"

"""@Brief: This script generates appsToBuild.yaml file from btmesh_production_template.xml.
The output includes all supported sample applications and bootloadersfor the given boards."""


class ExampleToGenerate:
    def __init__(self, path, name, board_type):
        self.path = path
        self.name = name
        self.board_type = board_type

    def __eq__(self, other):
        if isinstance(other, ExampleToGenerate):
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
    output_content,
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
            slcp_path = ExampleToGenerate(
                path=path, name=name, board_type=tb_board
            )
            output_content.append(slcp_path)


def crosscheck_production_template_and_boards(
    testbed_board_list,
    production_template_path,
    output_path,
    is_example=True,
):
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
                output_content=output_path,
                is_example=is_example,
            )
    if is_example:
        print("SLCPs supported on test boards and affected by change:")
    else:
        print("Bootloaders supported on test boards:")
    print()
    for gen in output_path:
        print(gen.name)


def main():
    args = parse_args()
    hw_mux_token = args.hwmux_token
    hw_mux_label = args.hwmux_label
    mux_handler: HwMuxApi = HwMuxApi(user_token=hw_mux_token)
    testbed_board_list = get_board_list(mux=mux_handler, label=hw_mux_label)
    testbed_board_list = [element.lower() for element in testbed_board_list]

    # Get all supported examples
    example_classes = list()
    crosscheck_production_template_and_boards(
        testbed_board_list,
        example_config_name,
        example_classes,
        is_example=True,
    )

    # Determine which bootloaders may be needed to run those applications
    # For now, generate all bootloaders only with "single", "spi" or "uart" in their names
    bootloader_classes = list()
    if not len(example_classes) == 0:
        crosscheck_production_template_and_boards(
            testbed_board_list,
            bootloader_example_config_name,
            bootloader_classes,
            is_example=False,
        )
    generated_classes = example_classes + bootloader_classes
    dump_to_yaml(output_file_name, generated_classes)

    print(f"Generated to: {os.path.abspath(output_file_name)}")
    print(f"Apps generated: {len(example_classes)}")
    print(f"Bootloaders generated: {len(bootloader_classes)}")


if __name__ == "__main__":
    main()
