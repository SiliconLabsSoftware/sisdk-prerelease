
from py_2_and_3_compatibility import *
from pyradioconfig.calculator_model_framework.Utils.LogMgr import LogMgr


class profile_MBus_modes(object):


    @staticmethod
    def set_agc_to_some_mysterious_values(model, family):
        if family not in ["dumbo","unit_test_part"]:
            model.vars.AGC_MININDEX_INDEXMINPGA.value_forced = 9
            model.vars.AGC_MININDEX_INDEXMINDEGEN.value_forced = 6
            model.vars.AGC_MININDEX_INDEXMINSLICES.value_forced = 0
            model.vars.AGC_MININDEX_INDEXMINATTEN.value_forced = 21

            model.vars.RAC_SGLNAMIXCTRL1_TRIMNPATHBW.value_forced = 3
            model.vars.RAC_IFPGACTRL1_LBWMODE.value_forced = 1
            model.vars.AGC_GAINSTEPLIM_SLOWDECAYCNT.value_forced = 30


    @staticmethod
    def profile_wMbus_ModeT_M2O_100k(model, family):

        model.vars.base_frequency_hz.value_forced = long(868950000)
        model.vars.bitrate.value_forced = 100000
        model.vars.baudrate_tol_ppm.value_forced = 120000
        model.vars.channel_spacing_hz.value_forced = 1000000
        model.vars.deviation.value_forced = 50000
        model.vars.frequency_comp_mode.value_forced = model.vars.frequency_comp_mode.var_enum.INTERNAL_LOCK_AT_PREAMBLE_DETECT
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.preamble_length.value_forced = 38
        model.vars.rx_xtal_error_ppm.value_forced = 20
        model.vars.tx_xtal_error_ppm.value_forced = 0

        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE

        model.vars.agc_hysteresis.value_forced = 0
        model.vars.agc_power_target.value_forced = -22
        model.vars.bandwidth_hz.value_forced = 315000
        model.vars.freq_offset_hz.value_forced = 0
        model.vars.number_of_timing_windows.value_forced = 3
        model.vars.symbols_in_timing_window.value_forced = 6
        model.vars.timing_detection_threshold.value_forced = 5
        model.vars.timing_sample_threshold.value_forced = 15
        model.vars.agc_settling_delay.value_forced = 40

        model.vars.MODEM_CTRL2_DEVWEIGHTDIS.value_forced = 1
        model.vars.MODEM_MODINDEX_FREQGAINE.value_forced = 2
        model.vars.MODEM_MODINDEX_FREQGAINM.value_forced = 3
        model.vars.RAC_IFFILTCTRL_CENTFREQ.value_forced = 2

        model.vars.AGC_CTRL2_FASTLOOPDEL.value_forced = 4
        model.vars.AGC_LOOPDEL_LNASLICESDEL.value_forced = 4
        model.vars.AGC_LOOPDEL_IFPGADEL.value_forced = 4

        if family in ["dumbo","unit_test_part"]:
            model.vars.AGC_GAINSTEPLIM_CFLOOPSTEPMAX.value_forced = 4
        else:
            profile_MBus_modes.set_agc_to_some_mysterious_values(model, family)
            model.vars.AGC_GAINSTEPLIM_CFLOOPSTEPMAX.value_forced = 0


    @staticmethod
    def profile_wMbus_ModeN2g_19p2k(model, family):
        model.vars.base_frequency_hz.value_forced = long(169437500)
        model.vars.baudrate_tol_ppm.value_forced = 100
        model.vars.bitrate.value_forced = 19200
        model.vars.channel_spacing_hz.value_forced = 1000000
        model.vars.deviation.value_forced = 2400
        model.vars.frequency_comp_mode.value_forced = model.vars.frequency_comp_mode.var_enum.INTERNAL_LOCK_AT_PREAMBLE_DETECT
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK4
        model.vars.preamble_length.value_forced = 16
        model.vars.rx_xtal_error_ppm.value_forced = 0
        model.vars.tx_xtal_error_ppm.value_forced = 0
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.Gaussian
        model.vars.shaping_filter_param.value_forced = 0.5

        model.vars.agc_hysteresis.value_forced = 0
        model.vars.bandwidth_hz.value_forced = 25000
        model.vars.if_frequency_hz.value_forced = 150000
        model.vars.symbols_in_timing_window.value_forced = 6
        if model.part_family.lower() in ['dumbo', 'jumbo', 'nerio', 'nixi', "unit_test_part"]:
            model.vars.pll_bandwidth_tx.value_forced = model.vars.pll_bandwidth_tx.var_enum.BW_2120KHz
        else:
            model.vars.pll_bandwidth_tx.value_forced = model.vars.pll_bandwidth_tx.var_enum.BW_2000KHz
        model.vars.timing_detection_threshold.value_forced = 10
        model.vars.timing_resync_period.value_forced = 2
        model.vars.timing_sample_threshold.value_forced = 20
        model.vars.agc_settling_delay.value_forced = 40

        model.vars.MODEM_MODINDEX_FREQGAINE.value_forced = 3
        model.vars.MODEM_MODINDEX_FREQGAINM.value_forced = 4

        model.vars.MODEM_CTRL4_DEVOFFCOMP.value_forced = 1

        if family in ["dumbo","unit_test_part"]:
            model.vars.MODEM_CTRL2_DATAFILTER.value_forced = 2
            model.vars.MODEM_CTRL4_ISICOMP.value_forced = 5

            model.vars.AGC_CTRL2_FASTLOOPDEL.value_forced = 4
            model.vars.AGC_LOOPDEL_IFPGADEL.value_forced = 4
            model.vars.AGC_LOOPDEL_LNASLICESDEL.value_forced = 4
            model.vars.AGC_GAINSTEPLIM_CFLOOPSTEPMAX.value_forced = 4

        else:
            profile_MBus_modes.set_agc_to_some_mysterious_values(model, family)


    @staticmethod
    def profile_wMbus_ModeC_M2O_100k(model, family):
        model.vars.base_frequency_hz.value_forced = long(868950000)
        model.vars.baudrate_tol_ppm.value_forced = 0
        model.vars.bitrate.value_forced = 100000
        model.vars.deviation.value_forced = 45000
        model.vars.channel_spacing_hz.value_forced = 1000000

        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.preamble_length.value_forced = 38

        model.vars.rx_xtal_error_ppm.value_forced = 0
        model.vars.tx_xtal_error_ppm.value_forced = 0

        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE # Amey changed from Gaussian to None.

        model.vars.timing_detection_threshold.value_forced = 20
        model.vars.timing_sample_threshold.value_forced = 12
        model.vars.agc_settling_delay.value_forced = 34
        model.vars.symbols_in_timing_window.value_forced = 14

        model.vars.frequency_comp_mode.value_forced = model.vars.frequency_comp_mode.var_enum.INTERNAL_ALWAYS_ON
        model.vars.bandwidth_hz.value_forced = 200000

        model.vars.agc_period.value_forced = 0

        if family in ["dumbo","unit_test_part"]:
            model.vars.agc_power_target.value_forced = -8
        else:
            profile_MBus_modes.set_agc_to_some_mysterious_values(model, family)
            model.vars.agc_speed.value_forced = model.vars.agc_speed.var_enum.FAST


    @staticmethod
    def profile_wMbus_ModeC_O2M_50k(model, family):
        model.vars.base_frequency_hz.value_forced = long(869525000)
        model.vars.baudrate_tol_ppm.value_forced = 0
        model.vars.bitrate.value_forced = 50000
        model.vars.deviation.value_forced = 25000
        model.vars.channel_spacing_hz.value_forced = 1000000

        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.preamble_length.value_forced = 38

        model.vars.rx_xtal_error_ppm.value_forced = 0
        model.vars.tx_xtal_error_ppm.value_forced = 0

        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.Gaussian
        model.vars.shaping_filter_param.value_forced = 0.5

        model.vars.agc_period.value_forced = 0
        model.vars.agc_speed.value_forced = model.vars.agc_speed.var_enum.FAST

        model.vars.frequency_comp_mode.value_forced = model.vars.frequency_comp_mode.var_enum.INTERNAL_ALWAYS_ON
        model.vars.bandwidth_hz.value_forced = 150000

        if family not in ["dumbo","unit_test_part"]:
            profile_MBus_modes.set_agc_to_some_mysterious_values(model, family)



    @staticmethod
    def profile_wMbus_ModeT_O2M_32p768k(model, family):
        model.vars.freq_offset_hz.value_forced = 0
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2

        model.vars.agc_hysteresis.value_forced = 0
        model.vars.agc_power_target.value_forced = -22
        model.vars.agc_settling_delay.value_forced = 40

        model.vars.base_frequency_hz.value_forced = long(868300000)
        model.vars.baudrate_tol_ppm.value_forced = 20000
        model.vars.bitrate.value_forced = 32768
        model.vars.channel_spacing_hz.value_forced = 300000
        model.vars.deviation.value_forced = 50000

        model.vars.bandwidth_hz.value_forced = 300000 #320000
        model.vars.frequency_comp_mode.value_forced = model.vars.frequency_comp_mode.var_enum.INTERNAL_LOCK_AT_PREAMBLE_DETECT

        model.vars.preamble_length.value_forced = 60
        model.vars.rx_xtal_error_ppm.value_forced = 60
        model.vars.tx_xtal_error_ppm.value_forced = 25
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE
        model.vars.symbols_in_timing_window.value_forced = 6

        model.vars.symbols_in_timing_window.value_forced = 8  # 8
        model.vars.number_of_timing_windows.value_forced = 2
        model.vars.timing_detection_threshold.value_forced = 12  # 12
        model.vars.errors_in_timing_window.value_forced = 1
        model.vars.timing_sample_threshold.value_forced = 6  # 6

        model.vars.MODEM_CTRL2_DEVWEIGHTDIS.value_forced = 1
        model.vars.MODEM_MODINDEX_FREQGAINE.value_forced = 3 #2
        model.vars.MODEM_MODINDEX_FREQGAINM.value_forced = 2 #3
        model.vars.AGC_CTRL2_FASTLOOPDEL.value_forced = 0  # 4
        model.vars.AGC_GAINSTEPLIM_CFLOOPSTEPMAX.value_forced = 0

        if family not in ["dumbo","unit_test_part"]:
            profile_MBus_modes.set_agc_to_some_mysterious_values(model, family)

        if family not in ["dumbo","jumbo","nerio","nixi","unit_test_part"]:
            model.vars.bitrate.value_forced = 16384

    @staticmethod
    def profile_wMbus_ModeS_32p768k(model, family):
        model.vars.freq_offset_hz.value_forced = 0
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2

        model.vars.agc_hysteresis.value_forced = 0
        model.vars.agc_power_target.value_forced = -22
        model.vars.agc_settling_delay.value_forced = 40

        model.vars.base_frequency_hz.value_forced = long(868300000)
        model.vars.baudrate_tol_ppm.value_forced = 20000
        model.vars.bitrate.value_forced = 32768
        model.vars.channel_spacing_hz.value_forced = 300000
        model.vars.deviation.value_forced = 50000

        model.vars.bandwidth_hz.value_forced = 300000  # 320000
        model.vars.frequency_comp_mode.value_forced = model.vars.frequency_comp_mode.var_enum.INTERNAL_LOCK_AT_PREAMBLE_DETECT
        model.vars.preamble_length.value_forced = 30

        model.vars.rx_xtal_error_ppm.value_forced = 60
        model.vars.tx_xtal_error_ppm.value_forced = 25

        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE
        model.vars.symbols_in_timing_window.value_forced = 6

        model.vars.symbols_in_timing_window.value_forced = 4  # original 6
        model.vars.number_of_timing_windows.value_forced = 3  # original 2
        model.vars.timing_detection_threshold.value_forced = 12
        model.vars.errors_in_timing_window.value_forced = 0   # original 1
        model.vars.timing_sample_threshold.value_forced = 6   # original 4 	set to 6 to improve PER floor at power level between -90dBm and -50dBm

        model.vars.MODEM_CTRL2_DEVWEIGHTDIS.value_forced = 1
        model.vars.MODEM_MODINDEX_FREQGAINE.value_forced = 3
        model.vars.MODEM_MODINDEX_FREQGAINM.value_forced = 2
        model.vars.AGC_CTRL2_FASTLOOPDEL.value_forced = 8  #origianl 0, set to 8 to improve PER floor at power lever > -50dBm
        model.vars.AGC_GAINSTEPLIM_CFLOOPSTEPMAX.value_forced = 0

        if family not in ["dumbo","unit_test_part"]:
            profile_MBus_modes.set_agc_to_some_mysterious_values(model, family)

        if family not in ["dumbo","jumbo","nerio","nixi","unit_test_part"]:
            model.vars.bitrate.value_forced = 16384


    @staticmethod
    def profile_wMbus_ModeN1a_4p8K(model, family):
        model.vars.freq_offset_hz.value_forced = 0
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2

        model.vars.agc_hysteresis.value_forced = 0
        model.vars.agc_power_target.value_forced = -22
        model.vars.agc_settling_delay.value_forced = 40

        model.vars.base_frequency_hz.value_forced = long(169406250)
        model.vars.baudrate_tol_ppm.value_forced = 100
        model.vars.bitrate.value_forced = 4800
        model.vars.channel_spacing_hz.value_forced = 12500
        model.vars.deviation.value_forced = 2400

        model.vars.preamble_length.value_forced = 16
        model.vars.rx_xtal_error_ppm.value_forced = 9
        model.vars.tx_xtal_error_ppm.value_forced = 9
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.Gaussian
        model.vars.shaping_filter_param.value_forced = 0.5

        model.vars.frequency_comp_mode.value_forced = model.vars.frequency_comp_mode.var_enum.INTERNAL_ALWAYS_ON
        model.vars.timing_detection_threshold.value_forced = 7# 12, 7
        model.vars.errors_in_timing_window.value_forced = 1 #1
        model.vars.timing_sample_threshold.value_forced = 8  # 16, 8

        model.vars.bandwidth_hz.value_forced = 12350  # 12000
        model.vars.AGC_CTRL2_FASTLOOPDEL.value_forced = 0
        model.vars.AGC_GAINSTEPLIM_CFLOOPSTEPMAX.value_forced = 0
        model.vars.AGC_CTRL1_AGCPERIOD.value_forced = 5

        if family not in ["dumbo","unit_test_part"]:
            profile_MBus_modes.set_agc_to_some_mysterious_values(model, family)


    @staticmethod
    def profile_wMbus_ModeN1c_2p4K(model, family):
        model.vars.freq_offset_hz.value_forced = 0
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2

        model.vars.agc_hysteresis.value_forced = 0
        model.vars.agc_power_target.value_forced = -22
        model.vars.agc_settling_delay.value_forced = 40

        model.vars.base_frequency_hz.value_forced = long(169431250)
        model.vars.baudrate_tol_ppm.value_forced = 100
        model.vars.bitrate.value_forced = 2400
        model.vars.channel_spacing_hz.value_forced = 12500
        model.vars.deviation.value_forced = 2400

        model.vars.frequency_comp_mode.value_forced = model.vars.frequency_comp_mode.var_enum.INTERNAL_ALWAYS_ON

        model.vars.preamble_length.value_forced = 16
        model.vars.rx_xtal_error_ppm.value_forced = 12
        model.vars.tx_xtal_error_ppm.value_forced = 12
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.Gaussian
        model.vars.shaping_filter_param.value_forced = 0.5

        model.vars.timing_detection_threshold.value_forced = 7  # 6
        model.vars.errors_in_timing_window.value_forced = 1 #2
        model.vars.timing_sample_threshold.value_forced = 6  # 6

        model.vars.bandwidth_hz.value_forced = 14000 #12500
        model.vars.AGC_CTRL2_FASTLOOPDEL.value_forced = 0
        model.vars.AGC_GAINSTEPLIM_CFLOOPSTEPMAX.value_forced = 0
        model.vars.AGC_CTRL1_AGCPERIOD.value_forced = 5

        if family not in ["dumbo","unit_test_part"]:
            profile_MBus_modes.set_agc_to_some_mysterious_values(model, family)


    @staticmethod
    def profile_wMbus_ModeR_4p8k(model, family):
        model.vars.freq_offset_hz.value_forced = 0
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2

        model.vars.agc_hysteresis.value_forced = 0
        model.vars.agc_power_target.value_forced = -22
        model.vars.agc_settling_delay.value_forced = 40

        model.vars.base_frequency_hz.value_forced = long(868330000)
        model.vars.baudrate_tol_ppm.value_forced = 20000
        model.vars.bitrate.value_forced = 4800
        model.vars.channel_spacing_hz.value_forced = 60000
        model.vars.deviation.value_forced = 6000

        model.vars.bandwidth_hz.value_forced = 20000
        model.vars.frequency_comp_mode.value_forced = model.vars.frequency_comp_mode.var_enum.AFC_LOCK_AT_PREAMBLE_DETECT

        model.vars.preamble_length.value_forced = 78
        model.vars.rx_xtal_error_ppm.value_forced = 20
        model.vars.tx_xtal_error_ppm.value_forced = 20
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE

        model.vars.number_of_timing_windows.value_forced = 2  # 3
        model.vars.symbols_in_timing_window.value_forced = 8
        model.vars.timing_detection_threshold.value_forced = 12
        model.vars.errors_in_timing_window.value_forced = 1
        model.vars.timing_sample_threshold.value_forced = 6

        if family not in ["dumbo","unit_test_part"]:
            profile_MBus_modes.set_agc_to_some_mysterious_values(model, family)

        if family not in ["dumbo","jumbo","nerio","nixi","unit_test_part"]:
            model.vars.bitrate.value_forced = 2400


    @staticmethod
    def profile_wMbus_ModeF_2p4k(model, family):
        model.vars.freq_offset_hz.value_forced = 0
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2

        model.vars.agc_hysteresis.value_forced = 0
        model.vars.agc_power_target.value_forced = -22
        model.vars.agc_settling_delay.value_forced = 40

        model.vars.base_frequency_hz.value_forced = long(433820000)
        model.vars.baudrate_tol_ppm.value_forced = 0
        model.vars.bitrate.value_forced = 2400
        model.vars.channel_spacing_hz.value_forced = 50000
        model.vars.deviation.value_forced = 5500

        model.vars.bandwidth_hz.value_forced = 16400
        model.vars.frequency_comp_mode.value_forced = model.vars.frequency_comp_mode.var_enum.AFC_LOCK_AT_PREAMBLE_DETECT

        model.vars.preamble_length.value_forced = 78
        model.vars.rx_xtal_error_ppm.value_forced = 16
        model.vars.tx_xtal_error_ppm.value_forced = 16
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE

        model.vars.number_of_timing_windows.value_forced = 2 #3
        model.vars.symbols_in_timing_window.value_forced = 8  # 8
        model.vars.timing_detection_threshold.value_forced = 12  # 12
        model.vars.errors_in_timing_window.value_forced = 1
        model.vars.timing_sample_threshold.value_forced = 6  # 6

        model.vars.MODEM_AFC_AFCSCALEE.value_forced = 2
        model.vars.MODEM_AFC_AFCSCALEM.value_forced = 3
        model.vars.MODEM_AFC_AFCDEL.value_forced = 5  # 5

        model.vars.MODEM_AFC_AFCRXCLR.value_forced = 0
        model.vars.MODEM_AFCADJLIM_AFCADJLIM.value_forced = 1800

        if family not in ["dumbo","unit_test_part"]:
            profile_MBus_modes.set_agc_to_some_mysterious_values(model, family)

