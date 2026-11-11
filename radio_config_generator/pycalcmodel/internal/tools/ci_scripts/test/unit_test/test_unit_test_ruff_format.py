import runpy
import subprocess
import sys
from enum import IntEnum

import pytest

from internal.tools.ci_scripts import ruff_format


class DummyCompletedProcess:
    def __init__(self, returncode=0, stdout="", stderr=""):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr


class ReturnCode(IntEnum):
    SUCCESS = 0
    ERROR = 2
    CUSTOM_1 = 1
    CUSTOM_2 = 3
    CUSTOM_3 = 17


def test_level_enum_values():
    assert ruff_format.Level.WARNING.value == "WARNING"
    assert ruff_format.Level.ERROR.value == "ERROR"


def test_run_format_check_invokes_ruff_and_returns_zero_on_success(monkeypatch, capsys):
    called = {}

    def fake_run(cmd, capture_output, text, check):
        called["cmd"] = cmd
        called["capture_output"] = capture_output
        called["text"] = text
        called["check"] = check
        return DummyCompletedProcess(returncode=ReturnCode.SUCCESS)

    monkeypatch.setattr(subprocess, "run", fake_run)

    rc = ruff_format.run_format_check(ruff_format.Level.ERROR)

    assert rc == ReturnCode.SUCCESS
    assert called["cmd"] == ["ruff", "format", "--check", "."]
    assert called["capture_output"] is True
    assert called["text"] is True
    assert called["check"] is False
    out, err = capsys.readouterr()
    assert out == ""
    assert err == ""


def test_run_format_check_warning_mode_returns_zero_and_prints_failure_details(monkeypatch, capsys):
    def fake_run(*args, **kwargs):
        return DummyCompletedProcess(
            returncode=ReturnCode.CUSTOM_1,
            stdout="would reformat a.py\n",
            stderr="ruff internal error\n",
        )

    monkeypatch.setattr(subprocess, "run", fake_run)

    rc = ruff_format.run_format_check(ruff_format.Level.WARNING)

    assert rc == ReturnCode.SUCCESS
    out, err = capsys.readouterr()
    assert out == ""
    assert "WARNING: Ruff formatting issues found." in err
    assert "Run: ruff format ." in err
    assert "would reformat a.py" in err
    assert "ruff internal error" in err


def test_run_format_check_error_mode_propagates_nonzero_return_code(monkeypatch, capsys):
    def fake_run(*args, **kwargs):
        return DummyCompletedProcess(returncode=ReturnCode.ERROR, stdout="", stderr="")

    monkeypatch.setattr(subprocess, "run", fake_run)

    rc = ruff_format.run_format_check(ruff_format.Level.ERROR)

    assert rc == ReturnCode.ERROR
    out, err = capsys.readouterr()
    assert out == ""
    assert "ERROR: Ruff formatting issues found." in err
    assert "Run: ruff format ." in err


def test_run_format_check_failure_without_stdout_or_stderr_prints_only_headers(monkeypatch, capsys):
    MESSAGE_LENGTH = 2

    def fake_run(*args, **kwargs):
        return DummyCompletedProcess(returncode=ReturnCode.CUSTOM_1, stdout="", stderr="")

    monkeypatch.setattr(subprocess, "run", fake_run)

    _ = ruff_format.run_format_check(ruff_format.Level.WARNING)

    out, err = capsys.readouterr()
    assert out == ""
    assert err.count("\n") == MESSAGE_LENGTH
    assert "WARNING: Ruff formatting issues found." in err
    assert "Run: ruff format ." in err


def test_main_parses_mode_and_calls_run_format_check(monkeypatch):
    monkeypatch.setattr(sys, "argv", ["ruff_format.py", "--mode", "WARNING"])

    called = {}

    def fake_run_format_check(level):
        called["level"] = level
        return ReturnCode.CUSTOM_3

    monkeypatch.setattr(ruff_format, "run_format_check", fake_run_format_check)

    rc = ruff_format.main()

    assert rc == ReturnCode.CUSTOM_3
    assert called["level"] == ruff_format.Level.WARNING


def test_main_missing_mode_exits_with_argparse_error(monkeypatch):
    monkeypatch.setattr(sys, "argv", ["ruff_format.py"])

    with pytest.raises(SystemExit) as ex:
        ruff_format.main()

    assert ex.value.code == ReturnCode.ERROR


def test_main_invalid_mode_exits_with_argparse_error(monkeypatch):
    monkeypatch.setattr(sys, "argv", ["ruff_format.py", "--mode", "INVALID"])

    with pytest.raises(SystemExit) as ex:
        ruff_format.main()

    assert ex.value.code == ReturnCode.ERROR


def test_dunder_main_raises_system_exit_with_main_return_code(monkeypatch):
    monkeypatch.setattr(sys, "argv", ["ruff_format.py", "--mode", "ERROR"])

    def fake_run(*args, **kwargs):
        return DummyCompletedProcess(returncode=ReturnCode.CUSTOM_2)

    monkeypatch.setattr(subprocess, "run", fake_run)

    with pytest.raises(SystemExit) as ex:
        runpy.run_path(ruff_format.__file__, run_name="__main__")

    assert ex.value.code == ReturnCode.CUSTOM_2
