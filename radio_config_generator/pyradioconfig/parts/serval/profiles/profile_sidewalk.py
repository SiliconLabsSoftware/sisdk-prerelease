from pyradioconfig.parts.nerio.profiles.profile_sidewalk import ProfileSidewalkNerio
from pyradioconfig.calculator_model_framework.interfaces.iprofile import IProfile
from pyradioconfig.parts.common.utils.units_multiplier import UnitsMultiplier
from pyradioconfig.parts.common.profiles.serval_regs import build_modem_regs_serval
from pyradioconfig.parts.common.profiles.profile_modem import buildModemInfoOutputs
from pyradioconfig.parts.common.profiles.profile_common import buildCrcOutputs, buildFecOutputs, buildFrameOutputs, \
    buildWhiteOutputs
from pyradioconfig.parts.serval.profiles.sw_profile_outputs_common import sw_profile_outputs_common_serval


class ProfileSidewalkServal(ProfileSidewalkNerio):

    fec_enabled_sync = 0x6F4E
    fec_disabled_sync = 0x904E #default

    def __init__(self):
        super().__init__()
        self._family = "serval"
        self._sw_profile_outputs_common = sw_profile_outputs_common_serval()

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

        self._sw_profile_outputs_common.buildStudioLogOutput(model, profile)

        return profile

    def build_optional_profile_inputs(self, model, profile):
        super().build_optional_profile_inputs(model, profile)
        self.make_optional_input(profile, model.vars.fec_tx_enable, 'Channel_Coding',
                                 readable_name="Enable FEC",
                                 default=model.vars.fec_tx_enable.var_enum.DISABLED)

    def build_hidden_profile_inputs(self, model, profile):
        IProfile.make_hidden_input(profile, model.vars.frame_length_type, 'frame_general',
                                   readable_name="Frame Length Algorithm")
        IProfile.make_hidden_input(profile, model.vars.fixed_length_size, category='frame_fixed_length',
                                   readable_name="Fixed Payload Size", value_limit_min=0, value_limit_max=0x7fffffff)

        IProfile.make_hidden_input(profile, model.vars.rx_xtal_error_ppm, category="general",
                                   readable_name="Set desired xtal tolerance on RX side", value_limit_min=0,
                                   value_limit_max=100)
        IProfile.make_hidden_input(profile, model.vars.tx_xtal_error_ppm, category="general",
                                   readable_name="Set desired xtal tolerance on TX side", value_limit_min=0,
                                   value_limit_max=100)
        IProfile.make_hidden_input(profile, model.vars.freq_offset_hz, 'Advanced',
                                   readable_name="Frequency Offset Compensation (AFC) Limit", value_limit_min=0,
                                   value_limit_max=500000, units_multiplier=UnitsMultiplier.KILO)
        IProfile.make_hidden_input(profile, model.vars.modulation_index_for_ksi, "Advanced",
                                   readable_name="Modulation index used by ksi calculation",
                                   value_limit_min=0.0, value_limit_max=5.0, fractional_digits=2)
        IProfile.make_hidden_input(profile, model.vars.trecs_optimize_cost_thd, "Advanced",
                                   readable_name="Enable conservative threshold calculation")
        IProfile.make_hidden_input(profile, model.vars.agc_lock_mode, 'Advanced', readable_name="AGC lock mode")
        IProfile.make_hidden_input(profile, model.vars.agc_enable_adc_attenuation, 'Advanced',
                                   readable_name="Enable ADC attenuation")
        IProfile.make_hidden_input(profile, model.vars.afc_run_mode, 'Advanced', readable_name="AFC Run mode")

    def build_register_profile_outputs(self, model, profile):
        build_modem_regs_serval(model, profile)
        buildFrameOutputs(model, profile)
        buildCrcOutputs(model, profile)
        buildWhiteOutputs(model, profile)
        buildFecOutputs(model, profile)

    def build_variable_profile_outputs(self, model, profile):
        self._sw_profile_outputs_common.build_rail_outputs(model, profile)
        self._sw_profile_outputs_common.build_ircal_outputs(model, profile)

    def _fixed_sidewalk_fec(self, model):
        pass

    def _set_sidewalk_tolerances(self, model):
        model.vars.baudrate_tol_ppm.value_forced = 300
        model.vars.rx_xtal_error_ppm.value_forced = 20
        model.vars.tx_xtal_error_ppm.value_forced = 20
        model.vars.deviation_tol_ppm.value_forced = 300000

    def _flexible_preamble_detection(self, model):
        model.vars.preamble_detection_length.value_forced = 32
        model.vars.MODEM_FRMSCHTIME_FRMSCHTIME.value_forced = 65535  # Disable fixed timeout
        model.vars.MODEM_REALTIMCFE_RTSCHMODE.value_forced = 1 #Need to use hard slicing for PMENDSCHEN to work
        model.vars.MODEM_FRMSCHTIME_PMENDSCHEN.value_forced = 1 #Enable timeout based on end of preamble

        model.vars.trecs_optimize_cost_thd.value_forced = False

    def _lookup_from_sidewalk_mode(self, model):

        sidewalk_mode = model.profile.inputs.sidewalk_mode.var_value
        fec_tx_enable = (model.profile.inputs.fec_tx_enable.var_value == model.vars.fec_tx_enable.var_enum.ENABLED)

        fec_factor = 2 if fec_tx_enable else 1

        if sidewalk_mode == model.vars.sidewalk_mode.var_enum.FSK_50kbps:
            model.vars.bitrate.value_forced = 50000//fec_factor
            model.vars.deviation.value_forced = 25000

        elif sidewalk_mode == model.vars.sidewalk_mode.var_enum.FSK_150kbps:
            model.vars.bitrate.value_forced = 150000//fec_factor
            model.vars.deviation.value_forced = 37500

        else:
            model.vars.bitrate.value_forced = 250000//fec_factor
            model.vars.deviation.value_forced = 62500
