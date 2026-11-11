from pyradioconfig.parts.common.profiles.ocelot_regs import build_modem_regs_ocelot
from pyradioconfig.parts.lynx.profiles.Profile_IEEE802154_OQPSK import ProfileIEEE802154OQPSKLynx
from pyradioconfig.parts.ocelot.profiles.sw_profile_outputs_common import sw_profile_outputs_common_ocelot
from pyradioconfig.calculator_model_framework.Utils.CustomExceptions import InvalidOptionOverride
from pycalcmodel.core.output import ModelOutput, ModelOutputType
from pyradioconfig.parts.common.profiles.profile_common import buildCrcOutputs, buildFecOutputs, buildFrameOutputs, \
    buildWhiteOutputs


class ProfileIEEE802154Ocelot(ProfileIEEE802154OQPSKLynx):

    def __init__(self):
        super().__init__()
        self._profileName = "IEEE802154"
        self._readable_name = "IEEE802154 Profile"
        self._category = ""
        self._description = "Profile used for IEEE802154 phys"
        self._default = False
        self._activation_logic = ""
        self._family = "ocelot"
        self._sw_profile_outputs_common = sw_profile_outputs_common_ocelot()

    def buildProfileModel(self, model):
        profile = self._makeProfile(model)

        self.build_required_profile_inputs(model, profile)
        self.build_optional_profile_inputs(model, profile)
        self.build_advanced_profile_inputs(model, profile)
        self.build_hidden_profile_inputs(model, profile)
        self.build_metadata_profile_inputs(model, profile)

        buildFrameOutputs(model, profile)
        buildCrcOutputs(model, profile)
        buildWhiteOutputs(model, profile)
        buildFecOutputs(model, profile)
        self.build_register_profile_outputs(model, profile)
        self.build_variable_profile_outputs(model, profile)
        self.build_info_profile_outputs(model, profile)

        self._sw_profile_outputs_common.buildStudioLogOutput(model, profile)

    def build_advanced_profile_inputs(self, model, profile):
        self.make_linked_io(profile, model.vars.thread_feature, 'Advanced', readable_name="Thread PHY Feature Selection")
        super().build_advanced_profile_inputs(model, profile)

    def build_register_profile_outputs(self, model, profile):
        build_modem_regs_ocelot(model, profile)

    def build_variable_profile_outputs(self, model, profile):
        self._sw_profile_outputs_common.build_rail_outputs(model, profile)

        self._sw_profile_outputs_common.build_ircal_outputs(model, profile)

    def build_info_profile_outputs(self, model, profile):
        self._sw_profile_outputs_common.build_info_outputs(model, profile)
        profile.outputs.append(ModelOutput(model.vars.base_frequency_hz, '', ModelOutputType.INFO, readable_name="Base Channel Frequency"))
        profile.outputs.append(ModelOutput(model.vars.channel_spacing_hz, '', ModelOutputType.INFO, readable_name="Channel Spacing"))
        profile.outputs.append(ModelOutput(model.vars.agc_scheme, '', ModelOutputType.INFO, readable_name="AGC backoff scheme"))
        profile.outputs.append(ModelOutput(model.vars.bandwidth_hz, '', ModelOutputType.INFO, readable_name="Acquisition Channel Bandwidth"))
        profile.outputs.append(ModelOutput(model.vars.etsi_cat1_compatible, '', ModelOutputType.INFO, readable_name="ETSI Category 1 Compatibility"))
        profile.outputs.append(ModelOutput(model.vars.firstframe_bitsperword, '', ModelOutputType.INFO, readable_name="Length of the First Word"))
        profile.outputs.append(ModelOutput(model.vars.if_frequency_hz, '', ModelOutputType.INFO, readable_name="IF Frequency"))
        profile.outputs.append(ModelOutput(model.vars.lo_injection_side, '', ModelOutputType.INFO, readable_name="Injection side"))
        profile.outputs.append(ModelOutput(model.vars.pll_bandwidth_rx, '', ModelOutputType.INFO, readable_name="PLL Bandwidth in RX mode"))
        profile.outputs.append(ModelOutput(model.vars.pll_bandwidth_tx, '', ModelOutputType.INFO, readable_name="PLL Bandwidth in TX mode"))
        profile.outputs.append(ModelOutput(model.vars.src_disable, '', ModelOutputType.INFO, readable_name="SRC Operation"))
        profile.outputs.append(ModelOutput(model.vars.var_length_loc, '', ModelOutputType.INFO, readable_name="Byte position of dynamic length byte"))
        profile.outputs.append(ModelOutput(model.vars.fast_detect_enable, '', ModelOutputType.INFO, readable_name="Fast preamble detect"))
        profile.outputs.append(ModelOutput(model.vars.fec_tx_enable, '', ModelOutputType.INFO, readable_name="Enable FEC"))
        profile.outputs.append(ModelOutput(model.vars.preamble_detection_length, '', ModelOutputType.INFO, readable_name="Preamble Detection Length"))
        profile.outputs.append(ModelOutput(model.vars.skip2ant, '', ModelOutputType.INFO, readable_name="Skip 2nd antenna check with phase demod antenna diversity"))

    def _build_feature_settings(self, model):
        zigbee_feature = model.profile.inputs.zigbee_feature.var_value
        thread_feature = model.profile.inputs.thread_feature.var_value
        is_zigbee = zigbee_feature not in (model.vars.zigbee_feature.var_enum.NONE, None)
        is_thread = thread_feature not in (model.vars.thread_feature.var_enum.NONE, None)

        if is_zigbee and is_thread:
            raise InvalidOptionOverride("Feature must be either Zigbee or Thread, not both !")
        if is_zigbee:
            self._build_zigbee_settings(model, zigbee_feature)
        elif is_thread:
            self._build_thread_settings(model, thread_feature)

    def _build_zigbee_settings(self, model, zigbee_feature):
        model.vars.protocol_id.value_forced = model.vars.protocol_id.var_enum.Zigbee
        if zigbee_feature == model.vars.zigbee_feature.var_enum.GB868_863:
            self._copy_model_variables_from_phy(model, 'PHY_IEEE802154_GB868_863MHz_PHR2')
        elif zigbee_feature == model.vars.zigbee_feature.var_enum.GB868_915:
            self._copy_model_variables_from_phy(model, 'PHY_IEEE802154_GB868_915MHz_PHR2')
        elif zigbee_feature == model.vars.zigbee_feature.var_enum.NA915_R23:
            self._copy_model_variables_from_phy(model, 'PHY_IEEE802154_915MHz_2GFSK_R23_NA')

    def _build_thread_settings(self, model, thread_feature):
        model.vars.protocol_id.value_forced = model.vars.protocol_id.var_enum.Thread
        chcfg_base_frequency_hz = model.profile.inputs.chcfg_base_frequency_hz.var_value

        if thread_feature == model.vars.thread_feature.var_enum.THREAD_SUBGHZ:
            match chcfg_base_frequency_hz:
                case 863500000:
                    self._copy_model_variables_from_phy(model, 'PHY_Internal_IEEE802154_868MHz_coh_thread_sf8')
                case 902500000:
                    self._copy_model_variables_from_phy(model, 'PHY_Internal_IEEE802154_915MHz_coh_thread_sf8')
                case _:
                    raise InvalidOptionOverride(f"Unsupported base frequency: {chcfg_base_frequency_hz}")
        else:
            raise InvalidOptionOverride(f"Unsupported THREAD feature: {thread_feature}")

    def _build_delay_settings(self, model):
        zigbee_feature = model.profile.inputs.zigbee_feature.var_value

        if zigbee_feature == model.vars.zigbee_feature.var_enum.GB868_863:
            model.vars.rx_sync_delay_ns.value_forced = 56500
            model.vars.rx_eof_delay_ns.value_forced = 56500
            model.vars.tx_sync_delay_ns.value_forced = 10000
            model.vars.tx_eof_delay_ns.value_forced = 0
        elif zigbee_feature == model.vars.zigbee_feature.var_enum.GB868_915:
            model.vars.rx_sync_delay_ns.value_forced = 56500
            model.vars.rx_eof_delay_ns.value_forced = 56500
            model.vars.tx_sync_delay_ns.value_forced = 10000
            model.vars.tx_eof_delay_ns.value_forced = 0
        elif zigbee_feature == model.vars.zigbee_feature.var_enum.NA915_R23:
            model.vars.rx_sync_delay_ns.value_forced = 56500
            model.vars.rx_eof_delay_ns.value_forced = 56500
            model.vars.tx_sync_delay_ns.value_forced = 2000
            model.vars.tx_eof_delay_ns.value_forced = 0