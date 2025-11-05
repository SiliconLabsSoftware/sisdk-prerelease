import argparse
import random
import os
import yaml
import re
from pathlib import Path
from hwmux_client.hwmux_api import HwMuxApi

output_file_name = "testsToRun.yaml"
apps_to_build = "appsToBuild.yaml"
"""@Brief: This script generates testsToRun.yaml file for SQA-pipeline in the pre-determined format. It takes HwMux
label, utf-branch and marker flag parameters into account."""


class TestbedBoard:
    def __init__(self, board_type, board_family, group_id):
        self.board_type = board_type
        self.board_family = board_family
        self.gid = group_id


class TestEntity:
    def __init__(self, test_suite_name, test_marker, board_types, test_bed_name):
        self.test_suite_name = test_suite_name
        self.harness = "dynamic_harness.yaml"
        self.executor_type = "local"
        self.publish_test_results = True
        self.test_marker = test_marker
        self.board_types = board_types
        self.test_bed_name = test_bed_name

    def _get_board_types_for_yaml(self):
        return ",".join(brd.board_type for brd in self.board_types)

    def to_dict(self):
        return {
            'testSuite': self.test_suite_name,
            'utfParams': [
                f'--harness {self.harness}',
                f'--executor_type {self.executor_type}',
                f'--publish_test_results {self.publish_test_results}',
                f'--pytest_command "pytest --tb=native tests -m {self.test_marker} --board_id {self._get_board_types_for_yaml()}"'
            ],
            'testbedName': [self.test_bed_name]
        }


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--hwmux_label", type=str, required=True, help="The label of hwmux group to use"
    )
    parser.add_argument(
        "--hwmux_token", type=str, required=True, help="The token key for HWMUX"
    )
    parser.add_argument(
        "--utf_branch",
        type=str,
        required=True,
        help="The utf branch to conduct the tests on",
    )
    args = parser.parse_args()
    return args


def get_board_list_and_group_names(token, label):
    brd_list = list()
    mux_handler = HwMuxApi(token)
    group_ids = mux_handler.labels_api.labels_list(name=label).results[0].device_groups
    groups = mux_handler.groups_api.groups_list(id__in=group_ids).results
    group_names = [group.name for group in groups]
    for gid, g in enumerate(groups):
        given_test_group = list()
        for dev in g.devices:
            brd_name = str(dev.part.part_no).removesuffix(str(dev.part.revision))
            brd_family = str(dev.part.part_family["name"])
            given_test_group.append(
                TestbedBoard(
                    board_type=brd_name,
                    board_family=brd_family,
                    group_id=group_ids[gid],
                )
            )
        brd_list.append(given_test_group)
    print(f"Board types found in testbed {label} are: ")
    for group in brd_list:
        print(str(group[0].gid), "ID Test group boards:")
        for board in group:
            print("Type:", board.board_type, "Family:", board.board_family)
    return brd_list, group_names


def get_affected_slcp_list():
    affected_slcps = list()
    with open(apps_to_build, "r") as apps_built:
        generated_slcps = yaml.safe_load(apps_built)
        if generated_slcps is None:
            with open(output_file_name, "w") as out_file:
                out_file.write("#No tests should run as there are no changes")
                return
        test_suites = generated_slcps["test_suites"][0]["test_suite"]
        if test_suites is None:
            return affected_slcps
        for test_suite in test_suites:
            slcp_name = str(os.path.basename(os.path.dirname(test_suite["app"])))
            if (
                    ("_ncp_" not in slcp_name)
                    and ("bootloader" not in slcp_name)
                    and (slcp_name not in affected_slcps)
            ):
                affected_slcps.append(slcp_name)
    return affected_slcps


def get_yaml_data(utf_branch, test_entities):
    data = {
        'testRepo': [
            {
                'testRepoName': "utf_app_bt_mesh",  # Git name of the test script repo
                'testProjectName': "utf",
                'testBranchName': f"{utf_branch}",  # Branch name of the test git repo
                'stackTitle': "bt_mesh",  # Title of stack, this will affect where your apps end up
                'testsToRun': test_entities  # Placeholder for tests to run
            }
        ]
    }
    return data


def get_header():
    return (f"""---
# All paths are relative to the workspace

#The following test structure is in progress, mapping of other stack tests may need more information/iteration structure that will
#be improved as we incorporate more tests and stacks into the multibranch pipeline engine based structure
"""
            )


def reorder_boards_to_match_constraints(test_group):
    if re.match(r'EFR32.*22.*', test_group[0].board_family):
        test_group[0], test_group[1] = test_group[1], test_group[0]
    return test_group


def get_test_entity(app_folder_name, board_types, test_bed_name):
    entity = TestEntity(app_folder_name + "_test", test_marker=app_folder_name + "_pr",
                        board_types=reorder_boards_to_match_constraints(board_types), test_bed_name=test_bed_name)
    return entity


def main():
    args = parse_args()
    hw_mux_token = args.hwmux_token
    hw_mux_label = args.hwmux_label
    utf_branch = args.utf_branch
    changed_slcps = get_affected_slcp_list()
    if changed_slcps is None:
        print("No tests should run as there are no changes")
    tb_board_list, group_names = get_board_list_and_group_names(hw_mux_token, hw_mux_label)

    i = 0

    test_entities = list()
    for slcp in changed_slcps:
        entity: TestEntity = get_test_entity(slcp, tb_board_list[i], group_names[i])
        test_entities.append(entity)
        i += 1
        i = i % len(tb_board_list)
    test_entities_dict = [entity.to_dict() for entity in test_entities]
    with open(output_file_name, "w", encoding="utf-8") as output_file:
        output_file.write(get_header())
        output_file.write(
            yaml.dump(get_yaml_data(utf_branch, test_entities_dict), default_style=False, width=150, indent=4))
    print(
        f"Tests for the following applications will be ran using {hw_mux_label} testbed:"
    )
    print(*changed_slcps, sep="\n")
    print(f"Output generated to: {os.path.abspath(output_file_name)}")


if __name__ == "__main__":
    main()
