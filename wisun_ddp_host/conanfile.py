import os
import shutil
import urllib.request

from conan import ConanFile
from conan.errors import ConanException
from conan.tools.files import download, update_conandata
from conan.tools.scm import Git

_DEMO_BOARDS = (
    "brd2705a",
    "brd4270a",
    "brd4270b",
    "brd4271a",
    "brd4272a",
    "brd4400a",
    "brd4400b",
    "brd4400c",
    "brd4401a",
    "brd4401b",
    "brd4401c",
)
_DEMO_APP = "wisun_ddp_app"
_DEMO_BINARY = "wisun_soc_ddp_ram"
_ARTIFACTORY_BASE = "https://artifactory.silabs.net/artifactory/gsdk-generic-production"
_COMMANDER_ZIP_URL = (
    "https://www.silabs.com/documents/public/software/SimplicityCommander-Linux.zip"
)


class DdpHostRecipe(ConanFile):
    name = "wisun_ddp_host"

    description = "Wi-SUN DDP host-side Python tooling."
    license = "www.silabs.com/about-us/legal/master-software-license-agreement"
    author = "Silicon Laboratories Inc."
    homepage = "https://github.com/SiliconLabsInternal/wisun"
    url = "https://github.com/SiliconLabsInternal/wisun"
    topics = ("silabs", "wi-sun", "ddp", "python", "tools")

    build_policy = "never"
    no_copy_source = True
    revision_mode = "scm"
    revision_mode_excluded = ["commander", "demos"]

    # Needed for Studio SLT.
    # Dictionary to declare properties
    options = {
      "compatibleVersion": ["ANY"],
      "subPackage": [True, False],
      "releaseNotesUrl": ["ANY"],
      "packageType": ["ANY"],
      "sdkLtsTag": ["ANY"]
    }

    # Needed for Studio SLT.
    # Dictionary to define properties values.
    # Alternative is to set values in def configure(self) of recipe
    default_options = {
      "compatibleVersion": "ANY",
      "subPackage": False,
      "releaseNotesUrl": "",
      "packageType": "tools",
      "sdkLtsTag": ""
    }

    def requirements(self):
        pass

    def layout(self):
        self.folders.source = "."
        self.folders.build = "."

    def set_version(self):
        self.version = os.environ.get("SL_VERSION", "0.0.0")

    def export(self):
        commit = Git(self).get_commit()
        update_conandata(
            self,
            {
                "sources": {
                    "commit": commit,
                    "url": "git@github.com:SiliconLabsInternal/wisun.git",
                }
            },
        )

    def package(self):
        self._package_demo_firmware()
        # Copy all files and directories from source folder to package folder
        for name in sorted(os.listdir(self.source_folder)):
            # Skip the test directory
            if name == "test" or name == "commander":
                continue
            # Skip the generated files by conan
            if "conan" in name and name.endswith(".sh"):
                continue
            src = os.path.join(self.source_folder, name)
            dst = os.path.join(self.package_folder, name)
            if os.path.isdir(src):
                shutil.copytree(src, dst, dirs_exist_ok=True)
            else:
                shutil.copy2(src, dst)

    def package_info(self):
        # Allow `import ddp`, `import service`, etc. when this package is in the runenv.
        self.runenv_info.prepend_path("PYTHONPATH", self.package_folder)
        # Listed as an SDK package so the .slconf exporter reports it and Circle of Life
        # ships it inside the Simplicity SDK instead of it being installed on its own.
        self.buildenv_info.append_path("SLC_SDK_PACKAGE_PATH", self.package_folder)

    def package_id(self):
        self.info.clear()

    def _commander_cli(self):
        if cli := os.environ.get("COMMANDER_CLI") or shutil.which("commander"):
            return cli
        commander_dir = os.path.join(self.source_folder, "commander")
        commander_zip = os.path.join(commander_dir, "commander.zip")
        os.makedirs(commander_dir, exist_ok=True)
        if not os.path.exists(commander_zip):
            download(self, _COMMANDER_ZIP_URL, filename=commander_zip)
        self.run(f'cd "{commander_dir}" && unzip -q {commander_zip} && cd SimplicityCommander-Linux'
                 f" && tar -xjf Commander-cli_linux_x86_64*.tar.bz"
                 f" && chmod +x commander-cli/commander-cli")
        return os.path.join(commander_dir, "SimplicityCommander-Linux", "commander-cli", "commander-cli")

    def _package_demo_firmware(self):
        """Download demo .s37 images and produce matching .bin files in demos/."""
        demos_dir = os.path.join(self.source_folder, "demos")
        os.makedirs(demos_dir, exist_ok=True)
        self._download_demo_s37_files(demos_dir)
        os.makedirs(self.build_folder, exist_ok=True)
        cmd = self._commander_cli()
        for board in _DEMO_BOARDS:
            stem = f"{_DEMO_BINARY}-{board}"
            self.run(f'"{cmd}" convert "{demos_dir}/{stem}.s37" -o "{demos_dir}/{stem}.bin"')

    def _fetch_public_url(self, url, dest):
        """Fetch a public URL without Conan/Artifactory credentials."""
        with urllib.request.urlopen(url) as resp, open(dest, "wb") as out:
            shutil.copyfileobj(resp, out)

    def _download_demo_s37_files(self, demos_dir):
        """Download pre-built demo .s37 images into demos_dir (flat layout)."""
        version = os.environ.get("GSDK_DEMO_VERSION", "2025.12.3")
        release = os.environ.get("GSDK_DEMO_RELEASE", "2025.12")
        zip_base = (
            f"{_ARTIFACTORY_BASE}/{release}/{version}/demo-applications.zip!/{_DEMO_APP}"
        )
        failures = []
        for board in _DEMO_BOARDS:
            filename = f"{_DEMO_BINARY}-{board}.s37"
            url = f"{zip_base}/{board}/{filename}"
            dest = os.path.join(demos_dir, filename)
            try:
                self.output.info(f"Downloading demo {filename} from Artifactory")
                self._fetch_public_url(url, dest)
            except Exception as exc:
                self.output.error(f"Failed to download '{url}': {exc}")
                failures.append(board)

        if failures:
            raise ConanException(
                f"Failed to download demo .s37 for boards: {', '.join(failures)}"
            )
        self.output.success(f"Downloaded {len(_DEMO_BOARDS)} demo .s37 files to demos/")