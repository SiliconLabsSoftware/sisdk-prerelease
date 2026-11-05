from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pyradioconfig.parts.common.phys.phy_common import PHY_COMMON_FRAME_154
from pyradioconfig.parts.ocelot.phys.Phys_Studio_Base import PHYS_Studio_Base_Ocelot
from py_2_and_3_compatibility import *
from pycalcmodel.core.common import IPCategory

class PHYS_Studio_Base_Sigfox_Ocelot(IPhy):

    ip_category = IPCategory.LPW

    def PHY_Sigfox_915MHz_2GFSK_600bps_RXOnly(self, model,phy_name=None):
        '''
        Sigfox downlink (Rx):
        The sigfox Radio specification requires to meet -126dBm 10% PER spec, current measurement is around -133dBm.
        Note an accurate and stable frequency (HFXO) is required (TCXO is strongly recommended).

        The Rx configuration has a special feature.
        As the base station measures the frequency on the uplink
        packet and adjusts the downlink frequency to it, there is no need to use any
        AFC algorithm in the receiver and the narrowest possible filter bandwidth can
        be configured.
        '''
        phy = PHYS_Studio_Base_Ocelot().PHY_Studio_868M_2GFSK_600bps_800(model,
                                                                         phy_name='PHY_Sigfox_915MHz_2GFSK_600bps_RXOnly',
                                                                         readable_name="Sigfox RX 915MHz 2GFSK 600bps")

        phy.profile_inputs.rx_xtal_error_ppm.value = 0
        phy.profile_inputs.tx_xtal_error_ppm.value = 0
        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)
        phy.profile_inputs.base_frequency_hz.value = long(905200000)

        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.Gaussian
        phy.profile_inputs.shaping_filter_param.value = 0.74

        phy.profile_inputs.preamble_pattern.value = 1
        phy.profile_inputs.preamble_pattern_len.value = 2
        phy.profile_inputs.preamble_length.value = 90

        phy.profile_inputs.syncword_0.value = long(0x3227)
        phy.profile_inputs.syncword_1.value = long(0x0)
        phy.profile_inputs.syncword_length.value = 14

        phy.profile_inputs.frame_bitendian.value = model.vars.frame_bitendian.var_enum.LSB_FIRST

        phy.profile_inputs.frame_length_type.value = model.vars.frame_length_type.var_enum.FIXED_LENGTH
        phy.profile_inputs.fixed_length_size.value = 15
        phy.profile_inputs.payload_white_en.value = False
        phy.profile_inputs.payload_crc_en.value = True

        phy.profile_inputs.header_en.value = False
        phy.profile_inputs.header_size.value = 1
        phy.profile_inputs.header_calc_crc.value = False
        phy.profile_inputs.header_white_en.value = False

        phy.profile_inputs.crc_poly.value = model.vars.crc_poly.var_enum.CRC_16  # CRC polynomial:   CRC_16
        phy.profile_inputs.crc_seed.value = long(0x00000000)  # CRC seed:         0x0000
        phy.profile_inputs.crc_byte_endian.value = model.vars.crc_byte_endian.var_enum.MSB_FIRST  # CRC byte order:   MSB first
        phy.profile_inputs.crc_bit_endian.value = model.vars.crc_bit_endian.var_enum.LSB_FIRST  # CRC bit order:    LSB first
        phy.profile_inputs.crc_input_order.value = model.vars.crc_input_order.var_enum.LSB_FIRST  # CRC engine feed:  LSB first
        phy.profile_inputs.crc_pad_input.value = False
        phy.profile_inputs.crc_invert.value = False

        phy.profile_inputs.fsk_symbol_map.value = model.vars.fsk_symbol_map.var_enum.MAP0

        return phy

    def PHY_Sigfox_868MHz_2GFSK_600bps_RXOnly(self, model,phy_name=None):
        '''
        Sigfox downlink (Rx):
        The sigfox Radio specification requires to meet -126dBm 10% PER spec, current measurement is around -133dBm.
        Note an accurate and stable frequency (HFXO) is required (TCXO is strongly recommended).

        The Rx configuration has a special feature.
        As the base station measures the frequency on the uplink
        packet and adjusts the downlink frequency to it, there is no need to use any
        AFC algorithm in the receiver and the narrowest possible filter bandwidth can
        be configured.
        '''
        phy = PHYS_Studio_Base_Ocelot().PHY_Studio_868M_2GFSK_600bps_800(model,
                                                                         phy_name='PHY_Sigfox_868MHz_2GFSK_600bps_RXOnly',
                                                                         readable_name="Sigfox RX 868MHz 2GFSK 600bps")

        phy.profile_inputs.rx_xtal_error_ppm.value = 0
        phy.profile_inputs.tx_xtal_error_ppm.value = 0
        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)
        phy.profile_inputs.base_frequency_hz.value = long(869525000)

        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.Gaussian
        phy.profile_inputs.shaping_filter_param.value = 0.74

        phy.profile_inputs.preamble_pattern.value = 1
        phy.profile_inputs.preamble_pattern_len.value = 2
        phy.profile_inputs.preamble_length.value = 90

        phy.profile_inputs.syncword_0.value = long(0x3227)
        phy.profile_inputs.syncword_1.value = long(0x0)
        phy.profile_inputs.syncword_length.value = 14

        phy.profile_inputs.frame_bitendian.value = model.vars.frame_bitendian.var_enum.LSB_FIRST

        phy.profile_inputs.frame_length_type.value = model.vars.frame_length_type.var_enum.FIXED_LENGTH
        phy.profile_inputs.fixed_length_size.value = 15
        phy.profile_inputs.payload_white_en.value = False
        phy.profile_inputs.payload_crc_en.value = True

        phy.profile_inputs.header_en.value = False
        phy.profile_inputs.header_size.value = 1
        phy.profile_inputs.header_calc_crc.value = False
        phy.profile_inputs.header_white_en.value = False

        phy.profile_inputs.crc_poly.value = model.vars.crc_poly.var_enum.CRC_16  # CRC polynomial:   CRC_16
        phy.profile_inputs.crc_seed.value = long(0x00000000)  # CRC seed:         0x0000
        phy.profile_inputs.crc_byte_endian.value = model.vars.crc_byte_endian.var_enum.MSB_FIRST  # CRC byte order:   MSB first
        phy.profile_inputs.crc_bit_endian.value = model.vars.crc_bit_endian.var_enum.LSB_FIRST  # CRC bit order:    LSB first
        phy.profile_inputs.crc_input_order.value = model.vars.crc_input_order.var_enum.LSB_FIRST  # CRC engine feed:  LSB first
        phy.profile_inputs.crc_pad_input.value = False
        phy.profile_inputs.crc_invert.value = False

        phy.profile_inputs.fsk_symbol_map.value = model.vars.fsk_symbol_map.var_enum.MAP0

        return phy
