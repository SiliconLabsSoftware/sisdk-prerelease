from pathlib import Path

import pytest

from ..BtMeshGenerator import generate, dcd_build, dcd_build_models_relations, dcd_validate, generate_profiles, write_logs, ModelCollection
from ..BtMeshLogger import GeneratorLogger, ErrorLevel


@pytest.fixture
def generator_dir_path():
    return Path(__file__).parent.parent


@pytest.fixture
def test_dir_path(generator_dir_path: Path):
    return generator_dir_path / "test"


@pytest.fixture
def dcd_input_path(test_dir_path: Path):
    return test_dir_path / "input"


@pytest.fixture
def dcd_output_path(test_dir_path: Path):
    dcd_output_path =  test_dir_path / "output"
    if not dcd_output_path.exists():
        dcd_output_path.mkdir()
    return dcd_output_path

@pytest.fixture(autouse=True)   # Always runs
def cleanup():
    yield                       # Do nothing, then run tests
    ModelCollection.reset()     # Cleanup after
    GeneratorLogger.reset()

@pytest.fixture
def dcd_mandatory_mdls_path(dcd_input_path: Path):
    return dcd_input_path / "mandatory_mdls" / "dcd_config.btmeshconf"

@pytest.fixture
def dcd_ids_path(dcd_input_path: Path):
    return dcd_input_path / "ids"

@pytest.fixture
def dcd_light_hsl_path(dcd_input_path: Path):
    return dcd_input_path / "light_hsl"

@pytest.fixture
def dcd_ncp_srtest_path(dcd_input_path: Path):
    return dcd_input_path / "ncp_srtest"

@pytest.fixture
def dcd_basic_lightness_path(dcd_input_path: Path):
    return dcd_input_path / "basic_lightness"

@pytest.fixture
def dcd_two_ctl_servers_path(dcd_input_path: Path):
    return dcd_input_path / "two_ctl_servers"

@pytest.fixture
def dcd_two_scene_servers_path(dcd_input_path: Path):
    return dcd_input_path / "two_scene_servers"

@pytest.fixture
def dcd_vendor_models_path(dcd_input_path: Path):
    return dcd_input_path / "vendor_models"

@pytest.fixture
def dcd_sig_and_vendor_models_path(dcd_input_path: Path):
    return dcd_input_path / "sig_and_vendor"

@pytest.fixture
def sig_model_descriptor_path(generator_dir_path: Path):
    return generator_dir_path / "btmesh_sig_model_descriptors.json"

def test_dcd_mandatory_mdls_pass(
    dcd_mandatory_mdls_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    generate(
        dcd_mandatory_mdls_path,
        dcd_output_path,
        [sig_model_descriptor_path]
    )
    assert len(GeneratorLogger.validation_markers) == 0
    assert GeneratorLogger.result_code == 0

def test_dcd_light_hsl_pass(
    dcd_light_hsl_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    generate(
        dcd_light_hsl_path,
        dcd_output_path,
        [sig_model_descriptor_path]
    )
    assert len(GeneratorLogger.validation_markers) == 0
    assert GeneratorLogger.result_code == 0

def test_dcd_id_relations_pass(
    dcd_ids_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    generate(
        dcd_ids_path,
        dcd_output_path,
        [sig_model_descriptor_path]
    )
    assert len(GeneratorLogger.validation_markers) == 0
    assert GeneratorLogger.result_code == 0

def test_dcd_basic_lightness_pass(
    dcd_basic_lightness_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    generate(
        dcd_basic_lightness_path,
        dcd_output_path,
        [sig_model_descriptor_path]
    )
    assert len(GeneratorLogger.validation_markers) == 0
    assert GeneratorLogger.result_code == 0

def test_dcd_two_ctls_pass(
    dcd_two_ctl_servers_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    generate(
        dcd_two_ctl_servers_path,
        dcd_output_path,
        [sig_model_descriptor_path]
    )
    assert len(GeneratorLogger.validation_markers) == 0
    assert GeneratorLogger.result_code == 0

def test_dcd_two_ctls_no_mandatory_model_fail(
    dcd_two_ctl_servers_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    dcd, profile_paths = dcd_build(dcd_two_ctl_servers_path, [sig_model_descriptor_path])
    dcd.remove_sig_model_from_elem(0, 0x0000)
    valid = dcd_validate(dcd)
    assert valid == False
    dcd_build_models_relations(dcd)
    generate_profiles(dcd_output_path, profile_paths, dcd)
    write_logs(dcd_output_path)
    assert len(GeneratorLogger.validation_markers) == 1
    assert GeneratorLogger.result_code == 1

def test_dcd_two_scenes_pass(
    dcd_two_scene_servers_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    generate(
        dcd_two_scene_servers_path,
        dcd_output_path,
        [sig_model_descriptor_path]
    )
    assert len(GeneratorLogger.validation_markers) == 0
    assert GeneratorLogger.result_code == 0

def test_dcd_two_scenes_no_corresponding_scene_setup_server_fail(
    dcd_two_scene_servers_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    dcd, profile_paths = dcd_build(dcd_two_scene_servers_path, [sig_model_descriptor_path])
    dcd.remove_sig_model_from_elem(0, 0x1204)
    valid = dcd_validate(dcd)
    assert valid == True
    dcd_build_models_relations(dcd)
    generate_profiles(dcd_output_path, profile_paths, dcd)
    write_logs(dcd_output_path)
    assert len(GeneratorLogger.validation_markers) == 1
    assert GeneratorLogger.result_code == 1

def test_dcd_two_scenes_no_extended_scene_server_fail(
    dcd_two_scene_servers_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    dcd, profile_paths = dcd_build(dcd_two_scene_servers_path, [sig_model_descriptor_path])
    # Remove scene server from element 2
    # It is extended by and corresponds with Scene Setup Server -> fail
    dcd.remove_sig_model_from_elem(2, 0x1203)
    valid = dcd_validate(dcd)
    assert valid == True
    dcd_build_models_relations(dcd)
    generate_profiles(dcd_output_path, profile_paths, dcd)
    write_logs(dcd_output_path)
    assert len(GeneratorLogger.validation_markers) == 2
    assert GeneratorLogger.result_code == 1

def test_dcd_two_scenes_no_lc_pass(
    dcd_two_scene_servers_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    dcd, profile_paths = dcd_build(dcd_two_scene_servers_path, [sig_model_descriptor_path])
    # Remove all three models from element 1
    dcd.remove_sig_model_from_elem(1, 0x1000)
    dcd.remove_sig_model_from_elem(1, 0x130f)
    dcd.remove_sig_model_from_elem(1, 0x1310)
    valid = dcd_validate(dcd)
    assert valid == True
    dcd_build_models_relations(dcd)
    generate_profiles(dcd_output_path, profile_paths, dcd)
    write_logs(dcd_output_path)
    assert len(GeneratorLogger.validation_markers) == 0
    assert GeneratorLogger.result_code == 0

def test_dcd_two_scenes_no_second_scene_pass(
    dcd_two_scene_servers_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    dcd, profile_paths = dcd_build(dcd_two_scene_servers_path, [sig_model_descriptor_path])
    # Remove all scene models from element 2
    # Generic level server and CTL temperature stays
    dcd.remove_sig_model_from_elem(2, 0x1203)
    dcd.remove_sig_model_from_elem(2, 0x1204)
    valid = dcd_validate(dcd)
    assert valid == True
    dcd_build_models_relations(dcd)
    generate_profiles(dcd_output_path, profile_paths, dcd)
    write_logs(dcd_output_path)
    assert len(GeneratorLogger.validation_markers) == 0
    assert GeneratorLogger.result_code == 0

def test_dcd_two_scenes_no_ctl_temp_fail(
    dcd_two_scene_servers_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    dcd, profile_paths = dcd_build(dcd_two_scene_servers_path, [sig_model_descriptor_path])
    # Remove CTL temp server and the extended generic level server model from element 2
    # Scene models stay
    # Element 0's Light CTL Server corresponds with CTL temp server -> fail
    dcd.remove_sig_model_from_elem(2, 0x1002)
    dcd.remove_sig_model_from_elem(2, 0x1306)
    valid = dcd_validate(dcd)
    assert valid == True
    dcd_build_models_relations(dcd)
    generate_profiles(dcd_output_path, profile_paths, dcd)
    write_logs(dcd_output_path)
    assert len(GeneratorLogger.validation_markers) == 1
    assert GeneratorLogger.result_code == 1

def test_dcd_light_hsl_light_server_missing_fail(
    dcd_light_hsl_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    dcd, profile_paths = dcd_build(dcd_light_hsl_path, [sig_model_descriptor_path])
    dcd.remove_sig_model_from_elem(0, 0x1300)
    valid = dcd_validate(dcd)
    assert valid == True
    dcd_build_models_relations(dcd)
    generate_profiles(dcd_output_path, profile_paths, dcd)
    write_logs(dcd_output_path)
    assert len(GeneratorLogger.validation_markers) == 3
    assert GeneratorLogger.result_code == 1

def test_dcd_ncp_srtest_pass(
    dcd_ncp_srtest_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path,
):
    generate(
        dcd_ncp_srtest_path,
        dcd_output_path,
        [sig_model_descriptor_path]
    )
    assert len(GeneratorLogger.validation_markers) == 0
    assert GeneratorLogger.result_code == 0

def test_dcd_vendor_models_pass(
    dcd_vendor_models_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path
):
    generate(
        dcd_vendor_models_path,
        dcd_output_path,
        [sig_model_descriptor_path]
    )
    assert len(GeneratorLogger.validation_markers) == 0
    assert GeneratorLogger.result_code == 0

def test_dcd_vendor_models_model_4_missing_fail(
    dcd_vendor_models_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path
):
    dcd, profile_paths = dcd_build(dcd_vendor_models_path, [sig_model_descriptor_path])
    dcd.remove_vendor_model_from_elem(0, 0xFFFC)
    valid = dcd_validate(dcd)
    assert valid == False
    write_logs(dcd_output_path)
    assert len(GeneratorLogger.validation_markers) == 1
    assert GeneratorLogger.result_code == 1

def test_dcd_vendor_models_primary_constraint_on_secondary_element_fail(
    dcd_vendor_models_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path
):
    dcd, profile_paths = dcd_build(dcd_vendor_models_path, [sig_model_descriptor_path])
    dcd.move_vendor_model(0xFFFE, 0, 1)
    valid = dcd_validate(dcd)
    assert valid == False
    write_logs(dcd_output_path)
    assert len(GeneratorLogger.validation_markers) == 1
    assert GeneratorLogger.result_code == 1

def test_dcd_sig_and_vendor_models_pass(
    dcd_sig_and_vendor_models_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path
):
    generate(
        dcd_sig_and_vendor_models_path,
        dcd_output_path,
        [sig_model_descriptor_path]
    )
    assert len(GeneratorLogger.validation_markers) == 0
    assert GeneratorLogger.result_code == 0

def test_dcd_vendor_model_on_same_element_fail(
    dcd_sig_and_vendor_models_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path
):
    dcd, profile_paths = dcd_build(dcd_sig_and_vendor_models_path, [sig_model_descriptor_path])
    dcd.move_vendor_model(0xFFFA, 1, 0)
    valid = dcd_validate(dcd)
    assert valid == True
    dcd_build_models_relations(dcd)
    generate_profiles(dcd_output_path, profile_paths, dcd)
    write_logs(dcd_output_path)
    assert len(GeneratorLogger.validation_markers) == 1
    assert GeneratorLogger.result_code == 1

def test_dcd_vendor_model_extended_sig_not_present_fail(
    dcd_sig_and_vendor_models_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path
):
    dcd, profile_paths = dcd_build(dcd_sig_and_vendor_models_path, [sig_model_descriptor_path])
    dcd.remove_sig_model_from_elem(0, 0x1300)
    valid = dcd_validate(dcd)
    assert valid == True
    dcd_build_models_relations(dcd)
    generate_profiles(dcd_output_path, profile_paths, dcd)
    write_logs(dcd_output_path)
    assert len(GeneratorLogger.validation_markers) == 4
    assert GeneratorLogger.result_code == 1

def test_dcd_vendor_model_not_present_pass(
    dcd_sig_and_vendor_models_path: Path,
    dcd_output_path: Path,
    sig_model_descriptor_path: Path
):
    dcd, profile_paths = dcd_build(dcd_sig_and_vendor_models_path, [sig_model_descriptor_path])
    dcd.remove_vendor_model_from_elem(1, 0xFFFA)
    valid = dcd_validate(dcd)
    assert valid == True
    dcd_build_models_relations(dcd)
    generate_profiles(dcd_output_path, profile_paths, dcd)
    write_logs(dcd_output_path)
    assert len(GeneratorLogger.validation_markers) == 0
    assert GeneratorLogger.result_code == 0