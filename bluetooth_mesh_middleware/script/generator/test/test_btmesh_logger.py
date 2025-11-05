import pytest

from ..BtMeshLogger import *

@pytest.fixture
def log_dir_path():
    return Path(__file__).parent.parent

@pytest.fixture(autouse=True)   # Always runs
def cleanup():
    yield                       # Do nothing, then run tests
    GeneratorLogger.reset()     # Cleanup after

def test_success(log_dir_path):
    GeneratorLogger.write_logs(log_dir_path)
    assert GeneratorLogger.result_code == 0
    assert len(GeneratorLogger.logs) == 1 # "Generation successful"

def test_log_error(log_dir_path):
    GeneratorLogger.add_log(1, ErrorLevel.ERROR, "Oh no, an error!", "", [])
    GeneratorLogger.write_logs(log_dir_path)
    assert len(GeneratorLogger.logs) == 1
    assert GeneratorLogger.result_code == 1

def test_log_two_errors(log_dir_path):
    GeneratorLogger.add_log(1, ErrorLevel.ERROR, "Oh no, an error!", "", [])
    GeneratorLogger.add_log(2, ErrorLevel.ERROR, "Oh no, another error!", "", [])
    GeneratorLogger.write_logs(log_dir_path)
    assert len(GeneratorLogger.logs) == 2
    assert GeneratorLogger.result_code == 1

def test_marker_error(log_dir_path):
    GeneratorLogger.add_marker(ErrorLevel.ERROR, "Oh no, an error!")
    GeneratorLogger.write_logs(log_dir_path)
    assert len(GeneratorLogger.validation_markers) == 1
    assert GeneratorLogger.result_code == 1

def test_log_warn_marker_error(log_dir_path):
    GeneratorLogger.add_log(1, ErrorLevel.WARN, "Just a warning", "", [])
    GeneratorLogger.add_marker(ErrorLevel.ERROR, "Oh no, an error!")
    GeneratorLogger.write_logs(log_dir_path)
    assert len(GeneratorLogger.logs) == 1
    assert GeneratorLogger.result_code == 1

def test_log_error_marker_warn(log_dir_path):
    GeneratorLogger.add_log(1, ErrorLevel.ERROR, "Just a warning", "", [])
    GeneratorLogger.add_marker(ErrorLevel.WARN, "Oh no, an error!")
    GeneratorLogger.write_logs(log_dir_path)
    assert len(GeneratorLogger.logs) == 1
    assert GeneratorLogger.result_code == 1