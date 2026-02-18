from pyradioconfig.parts.ocelot.calculators.calc_frame_detect import CALC_Frame_Detect_Ocelot
import math
from math import ceil


class Calc_Frame_Detect_Bobcat(CALC_Frame_Detect_Ocelot):
   def calc_timbases_val(self, model):
      # Run existing Ocelot logic
      super().calc_timbases_val(model)
      # JIRA MCUW_RADIO_CFG-3184: When ADPC (antenna-diversity parallel correlation) is enabled
      # on a LEGACY or COHERENT demodulator, set TIMINGBASE = ADPCWNDCNT.
      # This change cannot be applied on Ocelot, since ADPCEN is not available
      # Bobcat-only additions (locals that Ocelot doesn't have)
      demod_select = model.vars.demod_select.value
      basebits = model.vars.preamble_pattern_len_actual.value
      antdiv_parallel_corr_en = model.vars.antdiv_enable_parallel_correlation.value
      adpc_wndcnt = model.vars.MODEM_ADPC1_ADPCWNDCNT.value

      if (demod_select in (model.vars.demod_select.var_enum.LEGACY,model.vars.demod_select.var_enum.COHERENT) and antdiv_parallel_corr_en):
         model.vars.symbols_in_timing_window.value = int(adpc_wndcnt) * int(basebits)
