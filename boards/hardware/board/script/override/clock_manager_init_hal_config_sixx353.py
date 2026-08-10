import yaml
import os
import logging
from typing import Set

from siliconlabs.slc.board_gen.util.clock_util import (
    find_hfxo, find_lfxo, get_board_id, set_ctune_value,
)

logger = logging.getLogger(__name__)

board_data_file = os.path.dirname(__file__) + '/../util/board_data.yaml'
if os.path.isfile(board_data_file):
    with open(board_data_file, 'r') as f:
        board_data = yaml.safe_load(f)
else:
    raise Exception('Unable to load board data containing tuning information')


def compatible(provides: Set[str], hw) -> bool:
    if not hw.provides('device_generic_family_sixx353'):
        return False

    # The SIXX353 oscillator/tree headers only load when the freqplan feature is
    # selected (config_file condition on clock_manager_freqplan_*_cfg). Without it
    # there are no config options to write, so skip instead of aborting generation.
    if ('clock_manager_freqplan_default_cfg' not in provides
            and 'clock_manager_freqplan_one_socpll_cfg' not in provides):
        return False

    if find_hfxo(hw) or find_lfxo(hw):
        return True

    board_id = get_board_id(hw)
    return board_id in board_data


def configure(project, hw, _):
    board_id = get_board_id(hw)
    hfxo = find_hfxo(hw)
    lfxo = find_lfxo(hw)

    if hfxo:
        set_ctune_value(project, board_id, 'hfxo', 'SL_CLOCK_MANAGER_HFXO_CTUNE', hfxo.ctune, board_data)
        project.config('SL_CLOCK_MANAGER_HFXO_FREQ').value = hfxo.frequency
        project.config('SL_CLOCK_MANAGER_HFXO_EN').value = 'SL_CLOCK_MANAGER_HFXO_EN_ENABLE'

    if lfxo:
        set_ctune_value(project, board_id, 'lfxo', 'SL_CLOCK_MANAGER_LFXO_CTUNE', lfxo.ctune, board_data)
        project.config('SL_CLOCK_MANAGER_LFXO_EN').value = 1
