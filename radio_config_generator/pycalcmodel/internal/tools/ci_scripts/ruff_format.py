import argparse
import subprocess
import sys
from enum import Enum

from colorama import Fore, Style, just_fix_windows_console


class Level(str, Enum):
    WARNING = "WARNING"
    ERROR = "ERROR"


def _print_colored_bold(level: Level, message: str, color: str, file=sys.stderr) -> None:
    """
    Print a message in colored bold text if the level is WARNING.
    We use ERROR only in Jenkins, and Jenkins cannot display colored bold text properly.
    """
    if level == Level.WARNING:
        message = f"{Style.BRIGHT}{color}{message}{Style.RESET_ALL}"

    print(message, file=file)


def run_format_check(level: Level) -> int:
    result = subprocess.run(
        ["ruff", "format", "--check", "."],
        capture_output=True,
        text=True,
        check=False,
    )

    if level == Level.WARNING:
        color = Fore.YELLOW
        emoticon = "!"
    else:
        color = Fore.RED
        emoticon = "X"

    if result.returncode != 0:
        _print_colored_bold(level, f"{emoticon} {level.value}: Ruff formatting issues found.", color, file=sys.stderr)
        _print_colored_bold(level, f"{emoticon} Run: ruff format .", color, file=sys.stderr)
        output = result.stdout if result.stdout else result.stderr
        if output:
            _print_colored_bold(level, f"{emoticon} {output}", color, file=sys.stderr)

    if level == Level.WARNING:
        return 0
    else:
        return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description="Ruff format checker")
    parser.add_argument(
        "--mode",
        required=True,
        choices=[level.value for level in Level],
        help="Ruff formatting mode",
    )

    just_fix_windows_console()  # Fix Windows console for colorama

    args = parser.parse_args()
    level = Level(args.mode)

    return run_format_check(level)


if __name__ == "__main__":
    raise SystemExit(main())
