#!/usr/bin/env python3

"""SLC upgrade tester tool"""

import argparse
import dataclasses
import os
import subprocess
import time
import xml.etree.ElementTree as ET

@dataclasses.dataclass
class Project:
    """SLC Project representation with the relevant attributes"""
    name: str
    label: str
    path: str
    quality: str
    category: str
    boards: list[str]

failed_slcps = []
passed_slcps = []
impossible_slcps = []
def get_project_list(templates_xml_path) -> list[Project]:
    """Get a list of projects from the templates XML file"""
    project_list = []
    tree = ET.parse(templates_xml_path)
    root = tree.getroot()
    project_attributes = {}
    for item in root.iter():
        if item.tag == "descriptors":
            if project_attributes:
                project_list.append(Project(**project_attributes))
                project_attributes = {}
            project_attributes["name"] = item.get("name")
            project_attributes["label"] = item.get("label")
        elif item.get("key") == "projectFilePaths":
            project_attributes["path"] = os.path.join(os.path.dirname(templates_xml_path), item.get("value"))
        elif item.get("key") == "category":
            project_attributes["category"] = item.get("value")
        elif item.get("key") == "quality":
            project_attributes["quality"] = item.get("value")
        elif item.get("key") == "boardCompatibility":
            project_attributes["boards"] = item.get("value").split()
            if "com.silabs.board.none" in project_attributes["boards"]:
                project_attributes["boards"].remove("com.silabs.board.none")
    project_list.append(Project(**project_attributes))
    return project_list

def run(cmd, log_file):
    """Run a command and log the output"""
    log_file.write("-" * len(cmd) + "\n" + cmd + "\n" + "-" * len(cmd) + "\n")
    log_file.flush()
    print(f"Running command: {cmd}")  # Debugging: Print the command
    process = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        shell=True,
        universal_newlines=True
    )
    for line in process.stdout:
        print(line, end='')
        log_file.write(line)
    process.stdout.close()
    return_code = process.wait()
    if return_code:
        raise subprocess.CalledProcessError(return_code, cmd)


def main():
    """Test entry point"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", help="Source SDK to upgrade from")
    parser.add_argument("target", help="Target SDK to upgrade to")
    parser.add_argument("templates", help="Templates XML file path")
    parser.add_argument("-w", "--workdir", help="Working directory", default="ws")
    args = parser.parse_args()

    test_statistics = {
        "skipped": 0,
        "impossible": 0,
        "failed": 0,
        "passed": 0,
        "duration": -time.time()
    }
    project_list = get_project_list(args.templates)
    for project in project_list:
        print(f"Test {project.name}")
        if not project.boards or project.path.endswith(".slcw"):
            print("  skipped")
            test_statistics["skipped"] += 1
            continue
        # TODO: Implement more sophisticated board selection logic
        board = project.boards[0]
        print(f"  board: {board}")
        # Assumption: project names are unique
        out_dir = os.path.join(args.workdir, project.name + "_" + board)
        os.makedirs(out_dir, exist_ok=True)
        log_file_path = os.path.join(out_dir, "test.log")
        start = time.time()
        with open(log_file_path, "w", encoding="utf-8") as log_file:
            try:
                # Step 1: Generate project from source SDK
                cmd1 = f"slc generate --trust-totality -np -tlcn gcc -o makefile -nocp -s {args.source} -d {out_dir} -p {project.path} --with {board}"
                run(cmd1, log_file)
                # Step 2: Upgrade project to target SDK
                generated_slcp_file = next((f for f in os.listdir(out_dir) if f.endswith(".slcp")), None)
                generated_slcp = os.path.join(out_dir, generated_slcp_file)
                cmd2 = f"slc upgrade --trust-totality -s {args.target} -p {generated_slcp}"
                run(cmd2, log_file)
                # Step 3: Re-generate project from target SDK
                cmd3 = f"slc generate --trust-totality -s {args.target} -p {generated_slcp}"
                run(cmd3, log_file)
                # Step 4: Build upgraded project
                makefile = next((f for f in os.listdir(out_dir) if f.endswith(".Makefile")), None)
                cmd4 = f"make -C {out_dir} -f {makefile} -j"
                run(cmd4, log_file)
                print("  passed")
                test_statistics["passed"] += 1
                passed_slcps.append("Passed: " + project.name + " Test board: " + board)
            except subprocess.CalledProcessError as e:
                print(f"  error: {e.returncode}")
                # Upgrade status: impossible.
                if e.returncode == 242:
                    print("  impossible")
                    test_statistics["impossible"] += 1
                    impossible_slcps.append("Impossible: " + project.name + "Test board" + board)
                else:
                    print("  failed")
                    test_statistics["failed"] += 1
                    failed_slcps.append("Failed: " + project.name + " Test board: " + board)
            except KeyboardInterrupt:
                print("\nAbort test")
                break
        print(f"  duration: {time.time() - start}")
    test_statistics["duration"] += time.time()
    print("Test summary:")
    print("\n".join([f"  {key}: {value}" for key, value in test_statistics.items()]))

    # Write results and failed SLC projects to migration_results.txt in the workdir
    results_txt_path = os.path.join(args.workdir, "migration_results.txt")
    with open(results_txt_path, "w", encoding="utf-8") as results_file:
        results_file.write("Migration Test Summary:\n")
        results_file.write("\n".join([f"  {key}: {value}" for key, value in test_statistics.items()]))
        results_file.write("\n\n")
        results_file.write("Passed SLC projects:\n")
        results_file.write("\n".join(passed_slcps))
        results_file.write("\n\n")
        results_file.write("Impossible SLC projects:\n")
        results_file.write("\n".join(impossible_slcps))
        results_file.write("\n\n")
        results_file.write("Failed SLC projects:\n")
        results_file.write("\n".join(failed_slcps))


if __name__ == "__main__":
    main()
