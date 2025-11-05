import argparse
import base64
import os
import re

import urllib3
from ubai_client import ApiClient, Configuration
from ubai_client.apis import ArtifactApi
from ubai_client.models import ArtifactInput
from pathlib import Path

brd_type = ""
example_name = ""

"""@Brief: This script is responsible for uploading the generated applications into UBAI, in the correct format so
that the SQA-tests can find it based on name and metadata"""


class UbaiArtifact:
    def __init__(self, filepath, filename, metadata):
        self.filepath = filepath
        self.filename = filename
        self.metadata = metadata

    def __eq__(self, other):
        return (self.filepath == other.filepath and
                self.filename == other.filename and
                self.metadata == other.metadata)


def get_client():
    """
    Get API client for UBAI service

    Returns:
        Instance of ``ApiClient``

    """
    config = Configuration.get_default_copy()

    config.verify_ssl = False  # disabled SSL verification for now. Need to install certs on hosts/docker to enable this.
    config.discard_unknown_keys = True
    config.retries = urllib3.Retry(
        connect=5, status=5, backoff_factor=10, status_forcelist=[500, 502, 503, 504]
    )
    setattr(config.retries, "DEFAULT_BACKOFF_MAX", 30)

    client = ApiClient(configuration=config)

    return client


def upload(filepath, filename, metadata=None):
    """Upload a file and associated metadata to UBAI"""
    # disable warnings due to insecure connection to Nexus
    urllib3.disable_warnings()

    if metadata is None:
        metadata = {}

    artifact_api = ArtifactApi(api_client=get_client())

    with open(filepath, "rb") as f:
        data = f.read()

    artifact_input = ArtifactInput(
        name=filename,
        extension=os.path.splitext(filepath)[1],
        base64_content=base64.b64encode(data).decode("utf-8"),
        validate_metadata=False,
        metadata=metadata,
    )
    artifact = artifact_api.upload_artifact(payload=artifact_input)

    return artifact


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source_folder", type=str, required=True,
        help="The root folder containing the subfolders for the built examples"
    )
    parser.add_argument(
        "--branch", type=str, required=True, help="GSDK Branch name"
    )
    parser.add_argument(
        "--build_number", type=str, required=True, help="Environment build number"
    )
    parser.add_argument(
        "--stack", type=str, required=False, help="Stack name with capital letters", default="BT_MESH"
    )

    return parser.parse_args()


def main():
    global brd_type
    args = parse_args()
    src_folder = args.source_folder
    branch = args.branch
    build_number = args.build_number
    stack = args.stack
    ubai_artifacts = list()
    print("Upload script working dictionary:", os.path.abspath(src_folder))
    for root, dirs, files in os.walk(src_folder):
        for file in files:
            if file.endswith('.s37'):
                file_path = Path(root) / file
                compiler = "gcc" if "gcc" in str(file_path) else "iar" if "iar" in str(file_path) else ""
                app_name = file_path.stem
                name = app_name
                is_debug_artifact = False
                if "bootloader" not in app_name:
                    name = file_path.parts[1].split("-")[0]
                substr = re.split(r'[_-]', str(file_path))
                for b in substr:
                    if "brd" in b:
                        brd_type = b.split("\\")[0]
                    if "debug" in b:
                        is_debug_artifact = True
                if brd_type != "":
                    upload_app_name = app_name + "_" + brd_type
                else:
                    upload_app_name = app_name
                metadata_content = {
                    "branch": branch,
                    "build_number": build_number,
                    "stack": stack,
                    "app_name": upload_app_name,
                    "name": name,
                    "target": brd_type,
                    "compiler": compiler,
                    "ucCommand": f"--with {brd_type}",
                    "isDebugArtifact": f"{is_debug_artifact}",
                }
                artifact = UbaiArtifact(filepath=file_path, filename=name, metadata=metadata_content)
                if artifact not in ubai_artifacts:
                    ubai_artifacts.append(artifact)
    for af in ubai_artifacts:
        upload(af.filepath, af.filename, af.metadata)
        print(f"Uploaded: {af.filename} from {af.filepath}")


if __name__ == "__main__":
    main()
