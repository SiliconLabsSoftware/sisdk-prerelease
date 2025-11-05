import os
import sys
import time
import shutil
import ssl
import zipfile
import urllib.request, urllib.parse
import json
import xml.etree.ElementTree as ET
import argparse
import subprocess

_JENKINS_BASE_URL = "https://jenkins-cbs-gecko-sdk.silabs.net/job/Gecko_SDK_Suite/job"
_ARTIFACTORY_BASE_URL = "https://artifactory.silabs.net/ui/api/v1/download?repoKey=gsdk-generic-staging&path="
_BUILD_XML = "build.xml"
_ZIP_NAME = "gecko-sdk.zip"
status_ok = "ok"
status_err = "err"
unzip_subpath = "gecko_sdk"


def download_artifact(branch, target_dir):
    print(f"Downloading {branch} to {target_dir}")
    # download build.xml only if it hasn't been downloaded yet
    url = "{:s}/{:s}/{:s}".format(_ARTIFACTORY_BASE_URL,
                                  branch,
                                  _BUILD_XML)
    build_xml_path = os.path.join(target_dir, _BUILD_XML)
    if not os.path.exists(target_dir):
        os.makedirs(target_dir, exist_ok=True)
    print(f"Downloading '{_BUILD_XML}' from {url}...")
    try:
        with urllib.request.urlopen(url, context=ssl._create_unverified_context()) as response, \
            open(build_xml_path, 'wb') as out_file:
            shutil.copyfileobj(response, out_file)
        print(f"Downloaded '{_BUILD_XML}' to {build_xml_path}")
    except Exception as e:
        print(f"Failed to download '{_BUILD_XML}': {e}")
        return status_err
    _gecko_sdk_zip = os.path.join(target_dir, _ZIP_NAME)
    url = "{:s}/{:s}/{:s}".format(_ARTIFACTORY_BASE_URL,
                                  branch,
                                  _ZIP_NAME)
    print(f"Downloading '{_ZIP_NAME}' from {url}...")
    try:
        with urllib.request.urlopen(url, context=ssl._create_unverified_context()) as response, \
            open(_gecko_sdk_zip, 'wb') as out_file:
            shutil.copyfileobj(response, out_file)
            print(f"Downloaded '{_ZIP_NAME}' to {_gecko_sdk_zip}")
    except Exception as e:
        print(f"Failed to download '{_ZIP_NAME}': {e}")
        return status_err
    return status_ok


def unzip(zip_path):
    print(f"Unzipping '{zip_path}'...")
    # Note that zipfile doesn't properly preserve file permissions!
    try:
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            extract_path = os.path.dirname(zip_path)
            zip_ref.extractall(extract_path)
            print(f"Unzipped '{zip_path}' to '{extract_path}'")
    except Exception as e:
        print("An error occurred during unzip: " + str(e))
        return status_err
    return status_ok


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="btmesh migration test script")
    parser.add_argument('-s', '--source', dest='source', type=str, help='Source branch', required=True)
    parser.add_argument('-spath', '--source_installation_path', type=str, help="Path to install the source sdk to",
                        default="../../../../../source_sdk")
    args = parser.parse_args()
    if not os.path.isabs(args.source_installation_path):
        args.source_installation_path = os.path.abspath(args.source_installation_path)
    status = download_artifact(args.source, args.source_installation_path)
    if status != status_ok:
        exit(1)
    status = unzip(os.path.join(args.source_installation_path, _ZIP_NAME))
    if status != status_ok:
        exit(1)
