
from py_2_and_3_compatibility import *
from pyradioconfig.calculator_model_framework.Utils.LogMgr import LogMgr


class profile_MBus_modes(object):


    @staticmethod
    def set_agc_to_some_mysterious_values(model, family):
        model.vars.AGC_MININDEX_INDEXMINPGA.value_forced = 9
        model.vars.AGC_MININDEX_INDEXMINDEGEN.value_forced = 6
        model.vars.AGC_MININDEX_INDEXMINSLICES.value_forced = 0
        model.vars.AGC_MININDEX_INDEXMINATTEN.value_forced = 21

        model.vars.RAC_SGLNAMIXCTRL1_TRIMNPATHBW.value_forced = 3
        model.vars.RAC_IFPGACTRL1_LBWMODE.value_forced = 1
        model.vars.AGC_GAINSTEPLIM_SLOWDECAYCNT.value_forced = 30

    @staticmethod
    def profile_wMbus_ModeT_M2O_100k(model, family):

        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.bitrate.value_forced = 66666
        model.vars.baudrate_tol_ppm.value_forced = 120000
        model.vars.deviation.value_forced = 50000
        model.vars.preamble_detection_length.value_forced = 38
        model.vars.rx_xtal_error_ppm.value_forced = 25
        model.vars.tx_xtal_error_ppm.value_forced = 60
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE
        model.vars.shaping_filter_param.value_forced = 0.0

        model.vars.bandwidth_hz.value_forced = 330000

        model.vars.freq_dev_max.value_forced = 80000
        model.vars.freq_dev_min.value_forced = 40000
        model.vars.MODEM_BCRDEMODKSI_BCRKSI3.value_forced = 30
        model.vars.MODEM_BCRDEMODCTRL_BBPMDETEN.value_forced = 1
        model.vars.MODEM_FRMSCHTIME_PMENDSCHEN.value_forced = 1
        model.vars.MODEM_FRMSCHTIME_FRMSCHTIME.value_forced = 65535

    @staticmethod
    def profile_wMbus_ModeTC_M2O_100k(model, family):


        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.bitrate.value_forced = 66666
        model.vars.baudrate_tol_ppm.value_forced = 120000
        model.vars.deviation.value_forced = 50000
        model.vars.preamble_detection_length.value_forced = 38
        model.vars.rx_xtal_error_ppm.value_forced = 25
        model.vars.tx_xtal_error_ppm.value_forced = 60
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE
        model.vars.shaping_filter_param.value_forced = 0.0
        model.vars.bandwidth_hz.value_forced = 330000
        model.vars.freq_dev_max.value_forced = 80000
        model.vars.MODEM_BCRDEMODCTRL_BBPMDETEN.value_forced = 1
        model.vars.MODEM_FRMSCHTIME_PMENDSCHEN.value_forced = 1
        model.vars.MODEM_FRMSCHTIME_FRMSCHTIME.value_forced = 65535

        model.vars.freq_dev_min.value_forced = 33750                        # To accomodate C Mode M2O Specs
        model.vars.MODEM_BCRDEMODPMEXP_BCRCFECOSTTHD.value_forced = 168     # To resolve sensitivity issue at corner case (868.95MHz / 88kcps / 80kHz freqdev)





    @staticmethod
    def profile_wMbus_ModeN2g_19p2k(model, family):

        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK4
        model.vars.bitrate.value_forced = 19200
        model.vars.deviation.value_forced = 2400
        model.vars.preamble_detection_length.value_forced = 16

        model.vars.rx_xtal_error_ppm.value_forced = 15
        model.vars.tx_xtal_error_ppm.value_forced = 15
        model.vars.baudrate_tol_ppm.value_forced = 100

        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.Gaussian
        model.vars.shaping_filter_param.value_forced = 0.5


        model.vars.demod_select.value_forced = model.vars.demod_select.var_enum.BCR
        model.vars.MODEM_BCRDEMODPMEXP_BCRCFECOSTTHD.value_forced = 195    # Resolve inter-freqoffset inoperable regions
        model.vars.MODEM_BCRDEMODAFC1_GEARSW.value_forced = 2              # Resolve inter-freqoffset inoperable regions
        model.vars.MODEM_BCRDEMOD4FSK1_FDEVCOMPRATIO.value_forced = 38     # Maximize fdev tol

    @staticmethod
    def profile_wMbus_ModeC_M2O_100k(model, family):

        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.bitrate.value_forced = 100000
        model.vars.deviation.value_forced = 45000
        model.vars.preamble_detection_length.value_forced = 38

        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE
        model.vars.shaping_filter_param.value_forced = 0.0

        model.vars.rx_xtal_error_ppm.value_forced = 25
        model.vars.tx_xtal_error_ppm.value_forced = 25
        model.vars.baudrate_tol_ppm.value_forced = 0
        model.vars.deviation_tol_ppm.value_forced = 250000

    @staticmethod
    def profile_wMbus_ModeC_O2M_50k(model, family):

        model.vars.baudrate_tol_ppm.value_forced = 0
        model.vars.bitrate.value_forced = 50000
        model.vars.deviation.value_forced = 25000
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.preamble_detection_length.value_forced = 38
        model.vars.rx_xtal_error_ppm.value_forced = 25
        model.vars.tx_xtal_error_ppm.value_forced = 25
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.Gaussian
        model.vars.shaping_filter_param.value_forced = 0.5

        model.vars.MODEM_TRECPMDET_PMMINCOSTTHD.value_forced = 350

    @staticmethod
    def profile_wMbus_ModeT_O2M_32p768k(model, family):
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.bitrate.value_forced = 16384
        model.vars.baudrate_tol_ppm.value_forced = 20000
        model.vars.deviation.value_forced = 50000
        model.vars.preamble_detection_length.value_forced = 30
        model.vars.rx_xtal_error_ppm.value_forced = 60
        model.vars.tx_xtal_error_ppm.value_forced = 25
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE
        model.vars.shaping_filter_param.value_forced = 0.0

        model.vars.MODEM_BCRDEMODPMEXP_BCRCFECOSTTHD.value_forced = 200

        model.vars.freq_dev_max.value_forced = 80000
        model.vars.freq_dev_min.value_forced = 40000

        model.vars.MODEM_FRMSCHTIME_PMENDSCHEN.value_forced = 1
        model.vars.MODEM_FRMSCHTIME_FRMSCHTIME.value_forced = 65535

    @staticmethod
    def profile_wMbus_ModeS_32p768k(model, family):

        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.bitrate.value_forced = 16384
        model.vars.baudrate_tol_ppm.value_forced = 20000
        model.vars.deviation.value_forced = 50000
        model.vars.preamble_detection_length.value_forced = 30
        model.vars.rx_xtal_error_ppm.value_forced = 60
        model.vars.tx_xtal_error_ppm.value_forced = 25
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE
        model.vars.shaping_filter_param.value_forced = 0.0

        model.vars.MODEM_BCRDEMODPMEXP_BCRCFECOSTTHD.value_forced = 200

        model.vars.freq_dev_max.value_forced = 80000
        model.vars.freq_dev_min.value_forced = 40000

        model.vars.MODEM_FRMSCHTIME_PMENDSCHEN.value_forced = 1
        model.vars.MODEM_FRMSCHTIME_FRMSCHTIME.value_forced = 65535

    @staticmethod
    def profile_wMbus_ModeN1a_4p8K(model, family):
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.baudrate_tol_ppm.value_forced = 100
        model.vars.bitrate.value_forced = 4800
        model.vars.deviation.value_forced = 2400
        model.vars.preamble_detection_length.value_forced = 16
        model.vars.rx_xtal_error_ppm.value_forced = 9
        model.vars.tx_xtal_error_ppm.value_forced = 9
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.Gaussian
        model.vars.shaping_filter_param.value_forced = 0.5

    @staticmethod
    def profile_wMbus_ModeN1c_2p4K(model, family):
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.baudrate_tol_ppm.value_forced = 100
        model.vars.bitrate.value_forced = 2400
        model.vars.deviation.value_forced = 2400
        model.vars.preamble_detection_length.value_forced = 16
        model.vars.rx_xtal_error_ppm.value_forced = 12
        model.vars.tx_xtal_error_ppm.value_forced = 12
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.Gaussian
        model.vars.shaping_filter_param.value_forced = 0.5

    @staticmethod
    def profile_wMbus_ModeR_4p8k(model, family):
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.baudrate_tol_ppm.value_forced = 20000
        model.vars.bitrate.value_forced = 2400
        model.vars.deviation.value_forced = 6000
        model.vars.preamble_detection_length.value_forced = 78
        model.vars.rx_xtal_error_ppm.value_forced = 10
        model.vars.tx_xtal_error_ppm.value_forced = 10
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE
        model.vars.shaping_filter_param.value_forced = 0.0

    @staticmethod
    def profile_wMbus_ModeF_2p4k(model, family):
        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2
        model.vars.baudrate_tol_ppm.value_forced = 0
        model.vars.bitrate.value_forced = 2400
        model.vars.deviation.value_forced = 5500
        model.vars.preamble_detection_length.value_forced = 78
        model.vars.rx_xtal_error_ppm.value_forced = 16
        model.vars.tx_xtal_error_ppm.value_forced = 16
        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.NONE
        model.vars.shaping_filter_param.value_forced = 0.0

    @staticmethod
    def profile_wMbus_ModeN_6p4k(model, family):

        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK4
        model.vars.bitrate.value_forced = 6400
        model.vars.deviation.value_forced = 1060
        model.vars.preamble_detection_length.value_forced = 16

        model.vars.baudrate_tol_ppm.value_forced = 100
        model.vars.rx_xtal_error_ppm.value_forced = 9
        model.vars.tx_xtal_error_ppm.value_forced = 9

        model.vars.shaping_filter.value_forced = model.vars.shaping_filter.var_enum.Gaussian
        model.vars.shaping_filter_param.value_forced = 0.5

        model.vars.demod_select.value_forced = model.vars.demod_select.var_enum.BCR
        model.vars.bandwidth_hz.value_forced = 23000                        # Widens sensitivity bathtub curve for 130% fdev to meet spec

        if hasattr(model.profile.outputs, 'MODEM_BCRDEMODPMEXP_BCRCFECOSTTHD'):
            model.vars.MODEM_BCRDEMODPMEXP_BCRCFECOSTTHD.value_forced = 195     # Resolve inter-freqoffset inoperable regions
        if hasattr(model.profile.outputs, 'MODEM_BCRDEMOD4FSK1_FDEVCOMPRATIO'):
            model.vars.MODEM_BCRDEMOD4FSK1_FDEVCOMPRATIO.value_forced = 46      # Maximize fdev tol