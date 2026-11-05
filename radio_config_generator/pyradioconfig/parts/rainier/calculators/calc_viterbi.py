from pyradioconfig.parts.bobcat.calculators.calc_viterbi import Calc_Viterbi_Bobcat
from pyradioconfig.calculator_model_framework.Utils.LogMgr import LogMgr
from py_2_and_3_compatibility import *

class CalcViterbiRainier(Calc_Viterbi_Bobcat):
    acqwin_unit = 1

    def calc_trecs_optimize_cost_thd(self, model):
        trecs_enabled = model.vars.trecs_enabled.value
        protocol_id = model.vars.protocol_id.value
        is_ble = protocol_id == model.vars.protocol_id.var_enum.BLE

        enable_opt = trecs_enabled and not is_ble
        model.vars.trecs_optimize_cost_thd.value = enable_opt

    def calc_trecs_weak_syncword_optimization(self, model):
        trecs_enabled = model.vars.trecs_enabled.value
        protocol_id = model.vars.protocol_id.value
        is_ble = protocol_id == model.vars.protocol_id.var_enum.BLE

        enable_opt = trecs_enabled and not is_ble
        model.vars.trecs_weak_syncword_optimization.value = enable_opt

    def calc_pmoffset_reg(self, model):

        afc_oneshot_enabled = (model.vars.MODEM_AFC_AFCONESHOT.value == 1)
        rtschmode = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value
        osr = model.vars.MODEM_TRECSCFG_TRECSOSR.value

        if (rtschmode == 1) and afc_oneshot_enabled:
            pmoffset = 2
        else:
            pmoffset = osr * 2 + 2

        self._reg_write(model.vars.MODEM_TRECSCFG_PMOFFSET, pmoffset)

    def calc_realtimcfe_extenschbyp_reg(self, model):
        pass

    def calc_realtimcfe_rtschmode_reg(self, model):

        dualsync = model.vars.syncword_dualsync.value
        demod_select = model.vars.demod_select.value
        ber_force_fdm0 = model.vars.ber_force_fdm0.value
        pmdeten = model.vars.MODEM_PHDMODCTRL_PMDETEN.value
        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
                demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:
            if fast_detect_enable:
                rtschmode = 1
            else:
                rtschmode = 0  # 0 means detect timing again using syncword
        else:
            rtschmode = 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_RTSCHMODE, rtschmode)

    def calc_swcoeffen_reg(self, model):

        afc1shot_en = model.vars.MODEM_AFC_AFCONESHOT.value
        aox_en = model.vars.aox_enable.value == model.vars.aox_enable.var_enum.ENABLED
        enhanced_en = model.vars.demod_select.value == model.vars.demod_select.var_enum.ENHANCED_DSSS
        ksi3swen_donotccare = not afc1shot_en

        if afc1shot_en and aox_en:
            LogMgr.Error('both AFC oneshot and AoX cannot be simultaneously enabled')

        swcoeffen = 1 if (afc1shot_en or aox_en or enhanced_en) else 0 # affects the channel filter switching only
        ksi3swenable = afc1shot_en

        self._reg_write(model.vars.MODEM_CHFCTRL_SWCOEFFEN, swcoeffen)
        self._reg_write(model.vars.MODEM_VTCORRCFG1_KSI3SWENABLE, ksi3swenable, do_not_care=ksi3swen_donotccare)

    def calc_vtdemoden_reg(self, model):
        demod_sel = model.vars.demod_select.value
        dssscfe_combo = model.vars.MODEM_DIGMIXCTRL_DSSSCFECOMBO.value

        if (demod_sel == model.vars.demod_select.var_enum.TRECS_VITERBI
                or demod_sel == model.vars.demod_select.var_enum.TRECS_SLICER
                or dssscfe_combo):
            reg = 1
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_VITERBIDEMOD_VTDEMODEN, reg)

    def calc_pmexpectpatt_reg(self, model):
        pre_str = model.vars.preamble_string_actual.value
        mapfsk = model.vars.MODEM_CTRL0_MAPFSK.value
        demod_sel = model.vars.demod_select.value
        trecs_effective_preamble_len = model.vars.trecs_effective_preamble_len.value

        if demod_sel == model.vars.demod_select.var_enum.TRECS_VITERBI or demod_sel == model.vars.demod_select.var_enum.TRECS_SLICER or demod_sel == model.vars.demod_select.var_enum.BCR:

            effective_pre_str = pre_str[:trecs_effective_preamble_len] #This is the preamble once some bits are shifted to the syncword
            zero_filler_str = '0'*32 #Add 32 zeroes to the end to make sure we have a long enough string
            combined_str = effective_pre_str + zero_filler_str

            reg = int(combined_str[0:32],2)
            if mapfsk:
                reg ^= 0xFFFFFFFF
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_TRECPMPATT_PMEXPECTPATT, reg)

    def calc_demod_expect_patt_value(self, model):

        demod_select = model.vars.demod_select.value
        syncword0 = model.vars.MODEM_SYNC0_SYNC0.value
        mapfsk = model.vars.MODEM_CTRL0_MAPFSK.value
        trecs_pre_bits_to_syncword = model.vars.trecs_pre_bits_to_syncword.value
        preamble_string = model.vars.preamble_string_actual.value
        syncword_len = model.vars.syncword_length.value
        ber_force_sync = model.vars.ber_force_sync.value

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
            demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:

            if ber_force_sync:
                patt = 0x052bcbb8

            else:
                syncword_str_part = '{:032b}'.format(syncword0)[-syncword_len:] #Read the rightmost characters

                if trecs_pre_bits_to_syncword > 0:
                    preamble_str_part = preamble_string[-trecs_pre_bits_to_syncword:]  # Read the rightmost characters
                else:
                    preamble_str_part = ""

                effective_syncword_str = preamble_str_part+syncword_str_part[::-1]+'0'*32 #reverse syncword part only

                viterbi_demod_expect_patt = int(effective_syncword_str[0:32],2)
                patt = viterbi_demod_expect_patt

            if mapfsk:
                patt ^= 0xFFFFFFFF
        else:
            patt = long(0x123556B7)

        model.vars.viterbi_demod_expect_patt.value = patt
