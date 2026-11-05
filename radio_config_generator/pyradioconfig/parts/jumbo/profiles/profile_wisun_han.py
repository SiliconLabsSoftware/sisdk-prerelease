from pyradioconfig.calculator_model_framework.interfaces.iprofile import IProfile
from pyradioconfig.parts.common.utils.units_multiplier import UnitsMultiplier
from pyradioconfig.parts.common.profiles.jumbo_regs import build_modem_regs_jumbo
from pyradioconfig.parts.common.profiles.profile_modem import buildModemInfoOutputs, buildRailOutputs
from pyradioconfig.parts.common.profiles.profile_common import buildCrcOutputs, buildFecOutputs, buildFrameOutputs, \
    buildWhiteOutputs, build_ircal_sw_vars, buildStudioLogOutput
from pyradioconfig.calculator_model_framework.Utils.CustomExceptions import CalculationException
from pycalcmodel.core.output import ModelOutput, ModelOutputType
from pycalcmodel.core.common import IPCategory


class ProfileWisunHanJumbo(IProfile):

    ip_category = IPCategory.LPW
    phySunFskSfd_0_uncoded = 0b1001000001001110
    phySunFskSfd_1_uncoded = 0b0111101000001110
    phySunFskSfd_0_coded = 0b0110111101001110
    phySunFskSfd_1_coded = 0b0110001100101101

    def __init__(self):
        self._profileName = "wisun_han"
        self._readable_name = "Wi-SUN HAN Profile"
        self._category = ""
        self._description = "Profile used for Wi-SUN HAN PHYs"
        self._default = False
        self._activation_logic = ""
        self._family = "jumbo"

    def buildProfileModel(self, model):
        profile = self._makeProfile(model)

        self.build_required_profile_inputs(model, profile)
        self.build_optional_profile_inputs(model, profile)
        self.build_advanced_profile_inputs(model, profile)
        self.build_hidden_profile_inputs(model, profile)
        self.build_deprecated_profile_inputs(model, profile)

        self.build_register_profile_outputs(model, profile)
        self.build_variable_profile_outputs(model, profile)
        self.build_info_profile_outputs(model, profile)

        buildStudioLogOutput(model, profile)

        return profile

    def profile_calculate(self, model):
        self._fixed_wisun_vars(model)
        self._populate_wisun_mode_from_han_mode(model)
        self._lookup_from_wisun_mode(model)
        self._lookup_syncword_from_fec(model)

    def build_required_profile_inputs(self, model, profile):

        IProfile.make_required_input(profile, model.vars.wisun_reg_domain, "WiSUN",
                                     readable_name="Wi-SUN Regulatory Domain")

        IProfile.make_required_input(profile, model.vars.wisun_operating_class, "WiSUN",
                                     readable_name="Wi-SUN Operating Class", value_limit_min=0,
                                     value_limit_max=255)

        IProfile.make_required_input(profile, model.vars.wisun_han_mode, "WiSUN",
                                     readable_name="Wi-SUN Operating Mode")

        IProfile.make_required_input(profile, model.vars.xtal_frequency_hz, "crystal",
                                     readable_name="Crystal Frequency", value_limit_min=38000000,
                                     value_limit_max=40000000, units_multiplier=UnitsMultiplier.MEGA)

    def build_optional_profile_inputs(self, model, profile):
        IProfile.make_optional_input(profile, model.vars.shaping_filter, "modem", readable_name="Shaping Filter",
                                     default=model.vars.shaping_filter.var_enum.Gaussian)
        IProfile.make_optional_input(profile, model.vars.shaping_filter_param, "modem",
                                     readable_name="Shaping Filter Parameter (BT or R)", value_limit_min=0.3,
                                     value_limit_max=2.0, fractional_digits=2, default=2.0)
        IProfile.make_optional_input(profile, model.vars.payload_white_en, 'advanced',
                                     readable_name="Payload Whitening Enable", default=True)

    def build_advanced_profile_inputs(self, model, profile):
        IProfile.make_linked_io(profile, model.vars.base_frequency_hz, "advanced",
                                     readable_name="Base Channel Frequency", value_limit_min=100000000,
                                     value_limit_max=956000000, units_multiplier=UnitsMultiplier.MEGA)

        IProfile.make_linked_io(profile, model.vars.channel_spacing_hz, "advanced",
                                     readable_name="Channel Spacing", value_limit_min=0,
                                     value_limit_max=10000000,
                                     units_multiplier=UnitsMultiplier.KILO)

    def build_hidden_profile_inputs(self, model, profile):
        IProfile.make_hidden_input(profile, model.vars.frame_length_type, 'frame_general',
                                   readable_name="Frame Length Algorithm")
        IProfile.make_hidden_input(profile, model.vars.fixed_length_size, category='frame_fixed_length',
                                   readable_name="Fixed Payload Size", value_limit_min=0, value_limit_max=4095)

        IProfile.make_hidden_input(profile, model.vars.rx_xtal_error_ppm, category="general",
                                   readable_name="Set desired xtal tolerance on RX side", value_limit_min=0,
                                   value_limit_max=100)
        IProfile.make_hidden_input(profile, model.vars.tx_xtal_error_ppm, category="general",
                                   readable_name="Set desired xtal tolerance on TX side", value_limit_min=0,
                                   value_limit_max=100)
        IProfile.make_hidden_input(profile, model.vars.freq_offset_hz, 'Advanced',
                                   readable_name="Frequency Offset Compensation (AFC) Limit", value_limit_min=0,
                                   value_limit_max=500000, units_multiplier=UnitsMultiplier.KILO)
        IProfile.make_hidden_input(profile, model.vars.timing_detection_threshold, 'Advanced',
                                   readable_name="Timing Detection Threshold", value_limit_min=0,
                                   value_limit_max=255)
        IProfile.make_hidden_input(profile, model.vars.fec_tx_enable, 'Channel_Coding',
                                     readable_name="Enable FEC")

        bandwidth_limit_adc = 2530000
        IProfile.make_hidden_input(profile, model.vars.bandwidth_hz, 'Advanced',
                                   readable_name="Acquisition Channel Bandwidth", value_limit_min=0,
                                   value_limit_max=bandwidth_limit_adc, units_multiplier=UnitsMultiplier.KILO)
        IProfile.make_hidden_input(profile, model.vars.if_frequency_hz, 'Advanced',
                                   readable_name="IF Frequency", value_limit_min=70000, value_limit_max=1900000,
                                   units_multiplier=UnitsMultiplier.KILO)

    def build_deprecated_profile_inputs(self, model, profile):
        pass

    def build_register_profile_outputs(self, model, profile):
        family = self._family
        build_modem_regs_jumbo(model, profile)
        buildFrameOutputs(model, profile)
        buildCrcOutputs(model, profile)
        buildWhiteOutputs(model, profile)
        buildFecOutputs(model, profile)

    def build_variable_profile_outputs(self, model, profile):
        buildRailOutputs(model, profile)
        build_ircal_sw_vars(model, profile)
        profile.outputs.append(ModelOutput(model.vars.ircal_power_level, '', ModelOutputType.RAIL_CONFIG,
                                           readable_name='IR cal power level (amplitude)'))
        profile.outputs.append(ModelOutput(model.vars.wisun_phy_mode_id, '', ModelOutputType.RAIL_CONFIG,
                                           readable_name='WiSUN PhyModeID'))
        profile.outputs.append(ModelOutput(model.vars.wisun_mode_switch_phr, '', ModelOutputType.RAIL_CONFIG,
                                           readable_name='WiSUN Mode Switch PHR'))

    def build_info_profile_outputs(self, model, profile):
        buildModemInfoOutputs(model, profile)

    def _fixed_wisun_vars(self, model):


        self._fixed_wisun_thresholds(model)

        self._fixed_wisun_agc(model)

        self._fixed_wisun_fec(model)

        model.vars.frame_bitendian.value_forced = model.vars.frame_bitendian.var_enum.LSB_FIRST  # 802154 17.2.2 (Reference Modulator Diagram)
        model.vars.frame_length_type.value_forced = model.vars.frame_length_type.var_enum.VARIABLE_LENGTH  # 802154 20.2.2 (PHR Field Format)
        model.vars.var_length_bitendian.value_forced = model.vars.var_length_bitendian.var_enum.MSB_FIRST  # 802154 20.2.2 (PHR Field Format)
        model.vars.var_length_byteendian.value_forced = model.vars.var_length_byteendian.var_enum.MSB_FIRST  # Consistent with var_length_bitendian above
        model.vars.var_length_numbits.value_forced = 11  # Frame Length contained in bits 5-15 (11 total bits) of PHR
        model.vars.var_length_shift.value_forced = 0  # Since the length is stored MSB_FIRST, we don't need to shift as the MSB is in bit 15
        model.vars.var_length_maxlength.value_forced = 2047  # 11-bit field for Frame Length
        model.vars.var_length_minlength.value_forced = 0
        model.vars.var_length_includecrc.value_forced = True  # Length is the total number of octets contained in the PSDU
        model.vars.var_length_adjust.value_forced = 0

        model.vars.header_calc_crc.value_forced = False  # No CRC on the header itself
        model.vars.header_en.value_forced = True  # 802154 20.2.2 (PHR Field Format)
        model.vars.header_size.value_forced = 2  # Header size is 16 bits
        model.vars.header_white_en.value_forced = False  # When whitening is enabled it shall be applied only over the PSDU

        model.vars.fec_en.value_forced = model.vars.fec_en.var_enum.FEC_154G_NRNSC_INTERLEAVING

        model.vars.white_poly.value_forced = model.vars.white_poly.var_enum.PN9_802154
        model.vars.white_seed.value_forced = 0xFF  # This has been tested, need a 0 in position 8 due to EFR32 whitening architecture
        model.vars.white_output_bit.value_forced = 8

        self._fixed_wisun_crc(model)
        model.vars.payload_crc_en.value_forced = True
        model.vars.crc_bit_endian.value_forced = model.vars.crc_bit_endian.var_enum.MSB_FIRST
        model.vars.crc_byte_endian.value_forced = model.vars.crc_byte_endian.var_enum.MSB_FIRST
        model.vars.crc_input_order.value_forced = model.vars.crc_input_order.var_enum.LSB_FIRST

        model.vars.modulation_type.value_forced = model.vars.modulation_type.var_enum.FSK2

        model.vars.diff_encoding_mode.value_forced = model.vars.diff_encoding_mode.var_enum.DISABLED
        model.vars.dsss_chipping_code.value_forced = 0
        model.vars.dsss_len.value_forced = 0
        model.vars.dsss_spreading_factor.value_forced = 0
        model.vars.fsk_symbol_map.value_forced = model.vars.fsk_symbol_map.var_enum.MAP0  # 802154 Table 20.8 and 20.9
        model.vars.symbol_encoding.value_forced = model.vars.symbol_encoding.var_enum.NRZ

        model.vars.preamble_pattern.value_forced = 1  # 802154 20.2.1.1 (Preamble field)
        model.vars.preamble_pattern_len.value_forced = 2
        model.vars.syncword_length.value_forced = 16

        model.vars.baudrate_tol_ppm.value_forced = 300

        model.vars.asynchronous_rx_enable.value_forced = False
        model.vars.manchester_mapping.value_forced = model.vars.manchester_mapping.var_enum.Default
        model.vars.syncword_tx_skip.value_forced = False
        model.vars.test_ber.value_forced = False
        model.vars.frame_coding.value_forced = model.vars.frame_coding.var_enum.NONE
        model.vars.fixed_length_size.value_forced = 1
        model.vars.frame_type_0_filter.value_forced = True
        model.vars.frame_type_0_length.value_forced = 0
        model.vars.frame_type_0_valid.value_forced = False
        model.vars.frame_type_1_filter.value_forced = True
        model.vars.frame_type_1_length.value_forced = 0
        model.vars.frame_type_1_valid.value_forced = False
        model.vars.frame_type_2_filter.value_forced = True
        model.vars.frame_type_2_length.value_forced = 0
        model.vars.frame_type_2_valid.value_forced = False
        model.vars.frame_type_3_filter.value_forced = True
        model.vars.frame_type_3_length.value_forced = 0
        model.vars.frame_type_3_valid.value_forced = False
        model.vars.frame_type_4_filter.value_forced = True
        model.vars.frame_type_4_length.value_forced = 0
        model.vars.frame_type_4_valid.value_forced = False
        model.vars.frame_type_5_filter.value_forced = True
        model.vars.frame_type_5_length.value_forced = 0
        model.vars.frame_type_5_valid.value_forced = False
        model.vars.frame_type_6_filter.value_forced = True
        model.vars.frame_type_6_length.value_forced = 0
        model.vars.frame_type_6_valid.value_forced = False
        model.vars.frame_type_7_filter.value_forced = True
        model.vars.frame_type_7_length.value_forced = 0
        model.vars.frame_type_7_valid.value_forced = False
        model.vars.frame_type_bits.value_forced = 3
        model.vars.frame_type_loc.value_forced = 0
        model.vars.frame_type_lsbit.value_forced = 0

    def _fixed_wisun_crc(self, model):
        model.vars.crc_poly.value_forced = model.vars.crc_poly.var_enum.CCITT_16
        model.vars.crc_invert.value_forced = False
        model.vars.crc_pad_input.value_forced = False
        model.vars.crc_seed.value_forced = 0x00000000

    def _fixed_wisun_thresholds(self, model):
        model.vars.timing_sample_threshold.value_forced = 0
        model.vars.frequency_comp_mode.value_forced = model.vars.frequency_comp_mode.var_enum.INTERNAL_LOCK_AT_PREAMBLE_DETECT

    def _fixed_wisun_agc(self, model):
        model.vars.agc_speed.value_forced = model.vars.agc_speed.var_enum.FAST

    def _fixed_wisun_fec(self, model):
        model.vars.fec_tx_enable.value_forced = model.vars.fec_tx_enable.var_enum.DISABLED

    def _populate_wisun_mode_from_han_mode(self, model):

        wisun_han_mode = model.profile.inputs.wisun_han_mode.var_value

        if wisun_han_mode == model.vars.wisun_han_mode.var_enum.Mode1b:
            model.vars.wisun_mode.value_forced = model.vars.wisun_mode.var_enum.Mode1b
        elif wisun_han_mode == model.vars.wisun_han_mode.var_enum.Mode2b:
            model.vars.wisun_mode.value_forced = model.vars.wisun_mode.var_enum.Mode2b
        else:
            raise CalculationException("Error: Unsupported Wi-SUN HAN mode selected")

    def _lookup_from_wisun_mode(self, model):

        wisun_mode = model.vars.wisun_mode.value
        wisun_reg_domain = model.profile.inputs.wisun_reg_domain.var_value
        fec_tx_enable = (model.profile.inputs.fec_tx_enable.var_value == model.vars.fec_tx_enable.var_enum.ENABLED)

        fec_factor = 2 if fec_tx_enable else 1

        if wisun_mode == model.vars.wisun_mode.var_enum.Mode1a:
            model.vars.bitrate.value_forced = 50000 // fec_factor
            model.vars.deviation.value_forced = 12500
            model.vars.preamble_length.value_forced = 8*8

            model.vars.number_of_timing_windows.value_forced = 2
            model.vars.timing_detection_threshold.value_forced = 16
            model.vars.timing_resync_period.value_forced = 1
            model.vars.MODEM_MODINDEX_FREQGAINM.value_forced = 3
            model.vars.MODEM_MODINDEX_FREQGAINE.value_forced = 2

        elif wisun_mode == model.vars.wisun_mode.var_enum.Mode1b:
            model.vars.bitrate.value_forced = 50000 // fec_factor
            model.vars.deviation.value_forced = 25000
            model.vars.preamble_length.value_forced = 8 * 8

            if wisun_reg_domain == model.vars.wisun_reg_domain.var_enum.CN:
                model.vars.number_of_timing_windows.value_forced = 2 #Applies only to CN

        elif wisun_mode == model.vars.wisun_mode.var_enum.Mode2a:
            model.vars.bitrate.value_forced = 100000 // fec_factor
            model.vars.deviation.value_forced = 25000
            model.vars.preamble_length.value_forced = 8 * 8

            model.vars.timing_detection_threshold.value_forced = 30

        elif wisun_mode == model.vars.wisun_mode.var_enum.Mode2b:
            model.vars.bitrate.value_forced = 100000 // fec_factor
            model.vars.deviation.value_forced = 50000
            model.vars.preamble_length.value_forced = 15 * 8

        elif wisun_mode == model.vars.wisun_mode.var_enum.Mode3:
            model.vars.bitrate.value_forced = 150000 // fec_factor
            model.vars.deviation.value_forced = 37500
            model.vars.preamble_length.value_forced = 12 * 8

            model.vars.number_of_timing_windows.value_forced = 4

        elif wisun_mode == model.vars.wisun_mode.var_enum.Mode4a:
            model.vars.bitrate.value_forced = 200000//fec_factor
            model.vars.deviation.value_forced = 50000
            model.vars.preamble_length.value_forced = 12 * 8

            model.vars.MODEM_MODINDEX_FREQGAINM.value_forced = 7 #Only way to achieve this is with reg overrides currently
            model.vars.MODEM_MODINDEX_FREQGAINE.value_forced = 1
            model.vars.MODEM_CTRL2_DEVWEIGHTDIS.value_forced = 1

        elif wisun_mode == model.vars.wisun_mode.var_enum.Mode4b:
            model.vars.bitrate.value_forced = 200000//fec_factor
            model.vars.deviation.value_forced = 100000
            model.vars.preamble_length.value_forced = 12 * 8

        elif wisun_mode == model.vars.wisun_mode.var_enum.Mode5:
            model.vars.bitrate.value_forced = 300000//fec_factor
            model.vars.deviation.value_forced = 75000
            model.vars.preamble_length.value_forced = 24 * 8

            model.vars.MODEM_MODINDEX_FREQGAINM.value_forced = 6 #Only way to achieve this is with reg overrides currently
            model.vars.MODEM_MODINDEX_FREQGAINE.value_forced = 1
            model.vars.MODEM_CTRL2_DEVWEIGHTDIS.value_forced = 1

        if wisun_mode == model.vars.wisun_mode.var_enum.Mode1a:
            model.vars.rx_xtal_error_ppm.value_forced = 20
            model.vars.tx_xtal_error_ppm.value_forced = 20
        else:
            model.vars.rx_xtal_error_ppm.value_forced = 10
            model.vars.tx_xtal_error_ppm.value_forced = 10

    def _lookup_syncword_from_fec(self, model):

        if model.profile.inputs.fec_tx_enable.var_value is not None:
            fec_tx_enable = model.profile.inputs.fec_tx_enable.var_value
        else:
            fec_tx_enable = model.vars.fec_tx_enable.value

        if fec_tx_enable == model.vars.fec_tx_enable.var_enum.DISABLED:
            syncword_0 = self.phySunFskSfd_0_uncoded
            syncword_1 = 0x00000000
        else:
            syncword_0 = self.phySunFskSfd_0_coded
            syncword_1 = self.phySunFskSfd_0_uncoded

        model.vars.syncword_0.value_forced = syncword_0
        model.vars.syncword_1.value_forced = syncword_1