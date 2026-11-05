from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pyradioconfig.parts.common.phys.phy_common import PHY_COMMON_FRAME_154

from py_2_and_3_compatibility import *
from pycalcmodel.core.common import IPCategory

class PHYS_Sigfox(IPhy):

    ip_category = IPCategory.LPW


    def PHY_Sigfox_868MHz_DBPSK_100bps(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='100bps 868MHz DBPSK PHY', phy_name=phy_name)
        phy.profile_inputs.diff_encoding_mode.value = model.vars.diff_encoding_mode.var_enum.DISABLED
        phy.profile_inputs.symbol_encoding.value = model.vars.symbol_encoding.var_enum.NRZ
        phy.profile_inputs.modulation_type.value = model.vars.modulation_type.var_enum.DBPSK
        phy.profile_inputs.bitrate.value = 100
        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.Root_Raised_Cosine
        phy.profile_inputs.shaping_filter_param.value = 0.0
        phy.profile_inputs.dsss_chipping_code.value = long(0)
        phy.profile_inputs.dsss_len.value = 0
        phy.profile_inputs.dsss_spreading_factor.value = 0
        phy.profile_inputs.syncword_length.value = 13
        phy.profile_inputs.preamble_pattern.value = 1
        phy.profile_inputs.preamble_pattern_len.value = 2
        phy.profile_inputs.syncword_0.value = long(0x6B)
        phy.profile_inputs.syncword_1.value = long(0x0)
        phy.profile_inputs.agc_power_target.value = 4
        phy.profile_inputs.errors_in_timing_window.value = 1
        phy.profile_inputs.rx_xtal_error_ppm.value = 0
        phy.profile_inputs.tx_xtal_error_ppm.value = 0
        self._set_xtal_frequency(phy, xtal_freq_hz=38400000) 
        phy.profile_inputs.base_frequency_hz.value =  long(868000000)
        phy.profile_inputs.preamble_length.value = 16
        phy.profile_inputs.channel_spacing_hz.value = 0
        phy.profile_inputs.deviation.value = 0
        phy.profile_inputs.agc_hysteresis.value = 0
        phy.profile_inputs.agc_settling_delay.value = 40
        phy.profile_inputs.baudrate_tol_ppm.value = 0
        phy.profile_inputs.fsk_symbol_map.value = model.vars.fsk_symbol_map.var_enum.MAP0
        phy.profile_inputs.sqi_threshold.value = 0
        phy.profile_inputs.timing_sample_threshold.value = 0
        phy.profile_inputs.number_of_timing_windows.value = 0
        phy.profile_inputs.timing_detection_threshold.value = 75
        phy.profile_inputs.pll_bandwidth_tx.value = model.vars.pll_bandwidth_tx.var_enum.BW_3000KHz
        PHY_COMMON_FRAME_154(phy, model)

        return phy

    def PHY_Sigfox_915MHz_DBPSK_600bps(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='100bps 868MHz DBPSK PHY', phy_name=phy_name)
        phy.profile_inputs.diff_encoding_mode.value = model.vars.diff_encoding_mode.var_enum.DISABLED
        phy.profile_inputs.symbol_encoding.value = model.vars.symbol_encoding.var_enum.NRZ
        phy.profile_inputs.modulation_type.value = model.vars.modulation_type.var_enum.DBPSK
        phy.profile_inputs.bitrate.value = 600
        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.Root_Raised_Cosine
        phy.profile_inputs.shaping_filter_param.value = 0.0
        phy.profile_inputs.dsss_chipping_code.value = long(0)
        phy.profile_inputs.dsss_len.value = 0
        phy.profile_inputs.dsss_spreading_factor.value = 0
        phy.profile_inputs.syncword_length.value = 13
        phy.profile_inputs.preamble_pattern.value = 1
        phy.profile_inputs.preamble_pattern_len.value = 2
        phy.profile_inputs.syncword_0.value = long(0x6B)
        phy.profile_inputs.syncword_1.value = long(0x0)
        phy.profile_inputs.agc_power_target.value = 4
        phy.profile_inputs.errors_in_timing_window.value = 1
        phy.profile_inputs.rx_xtal_error_ppm.value = 0
        phy.profile_inputs.tx_xtal_error_ppm.value = 0
        self._set_xtal_frequency(phy, xtal_freq_hz=38400000) 
        phy.profile_inputs.base_frequency_hz.value =  long(915000000)
        phy.profile_inputs.preamble_length.value = 16
        phy.profile_inputs.channel_spacing_hz.value = 0
        phy.profile_inputs.deviation.value = 0
        phy.profile_inputs.agc_hysteresis.value = 0
        phy.profile_inputs.agc_settling_delay.value = 40
        phy.profile_inputs.baudrate_tol_ppm.value = 0
        phy.profile_inputs.fsk_symbol_map.value = model.vars.fsk_symbol_map.var_enum.MAP0
        phy.profile_inputs.sqi_threshold.value = 0
        phy.profile_inputs.timing_sample_threshold.value = 0
        phy.profile_inputs.number_of_timing_windows.value = 0
        phy.profile_inputs.timing_detection_threshold.value = 75
        phy.profile_inputs.pll_bandwidth_tx.value = model.vars.pll_bandwidth_tx.var_enum.BW_3000KHz
        PHY_COMMON_FRAME_154(phy, model)

        return phy
        
    def PHY_Sigfox_915MHz_GFSK_600bps_WIP(self, model, phy_name=None):

        '''
        Sigfox downlink (Rx):
        This PHY still needs to be improved to meet -126dBm 10% PER spec, current measurement is arount -124dBm.

        The Rx configuration
        has a special feature. As the base station measures the frequency on the uplink
        packet and adjusts the downlink frequency to it, there is no need to use any
        AFC algorithm in the receiver and the narrowest possible filter bandwidth can
        be configured. That said the specification is still quite challenging at -126
        dBm at 10% PER.
        '''
        phy = self._makePhy(model, model.profiles.Base, readable_name='Sigfox 915MHz GFSK 600bps PHY', phy_name=phy_name)

        self._set_xtal_frequency(phy, xtal_freq_hz=38400000) 
        phy.profile_inputs.base_frequency_hz.value =  long(915000000)
        phy.profile_inputs.channel_spacing_hz.value = 0             # spacing is too narrow to use the spacing parameter
        phy.profile_inputs.baudrate_tol_ppm.value = 0

        phy.profile_inputs.bandwidth_hz.value = 2700
        phy.profile_inputs.if_frequency_hz.value = 400000

        phy.profile_inputs.rx_xtal_error_ppm.value = 0
        phy.profile_inputs.tx_xtal_error_ppm.value = 0

        phy.profile_inputs.modulation_type.value = model.vars.modulation_type.var_enum.FSK2
        phy.profile_inputs.bitrate.value = 600
        phy.profile_inputs.deviation.value = 800

        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.Gaussian
        phy.profile_inputs.shaping_filter_param.value = 1.0

        phy.profile_inputs.symbols_in_timing_window.value = 16
        phy.profile_inputs.agc_period.value = 0
        phy.profile_inputs.agc_speed.value = model.vars.agc_speed.var_enum.FAST


        phy.profile_inputs.preamble_pattern.value = 1               #
        phy.profile_inputs.preamble_pattern_len.value = 2           #
        phy.profile_inputs.preamble_length.value = 40               #

        phy.profile_inputs.syncword_0.value = long(0x1227)               #
        phy.profile_inputs.syncword_1.value = long(0x0)                  #
        phy.profile_inputs.syncword_length.value = 13               #


        phy.profile_inputs.frame_bitendian.value = model.vars.frame_bitendian.var_enum.LSB_FIRST


        phy.profile_inputs.frame_length_type.value = model.vars.frame_length_type.var_enum.FIXED_LENGTH
        phy.profile_inputs.fixed_length_size.value = 16
        phy.profile_inputs.payload_white_en.value = False
        phy.profile_inputs.payload_crc_en.value = True

        phy.profile_inputs.header_en.value = False
        phy.profile_inputs.header_size.value = 0
        phy.profile_inputs.header_calc_crc.value = False
        phy.profile_inputs.header_white_en.value = False

        phy.profile_inputs.crc_poly.value = model.vars.crc_poly.var_enum.CRC_16                     # CRC polynomial:   CRC_16
        phy.profile_inputs.crc_seed.value =  long(0x00000000)                                             # CRC seed:         0x0000
        phy.profile_inputs.crc_byte_endian.value = model.vars.crc_byte_endian.var_enum.MSB_FIRST    # CRC byte order:   MSB first
        phy.profile_inputs.crc_bit_endian.value = model.vars.crc_bit_endian.var_enum.LSB_FIRST      # CRC bit order:    LSB first
        phy.profile_inputs.crc_input_order.value = model.vars.crc_input_order.var_enum.LSB_FIRST    # CRC engine feed:  LSB first
        phy.profile_inputs.crc_pad_input.value = False
        phy.profile_inputs.crc_invert.value = False

        phy.profile_inputs.diff_encoding_mode.value = model.vars.diff_encoding_mode.var_enum.DISABLED
        phy.profile_inputs.dsss_chipping_code.value = long(0)
        phy.profile_inputs.dsss_len.value = 0
        phy.profile_inputs.dsss_spreading_factor.value = 0
        phy.profile_inputs.fsk_symbol_map.value = model.vars.fsk_symbol_map.var_enum.MAP0


        phy.profile_inputs.timing_detection_threshold.value = 4
        phy.profile_outputs.MODEM_CF_DEC1GAIN.override = 1
        phy.profile_inputs.timing_sample_threshold.value = 2
        phy.profile_inputs.freq_offset_hz.value = 0


        phy.profile_inputs.errors_in_timing_window.value = 1
        phy.profile_inputs.agc_settling_delay.value = 24

        phy.profile_inputs.in_2fsk_opt_scope.value = False

        return phy