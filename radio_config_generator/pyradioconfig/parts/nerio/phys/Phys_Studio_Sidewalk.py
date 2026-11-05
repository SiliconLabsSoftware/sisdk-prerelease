from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pycalcmodel.core.common import IPCategory

class PhysStudioSidewalkNerio(IPhy):

    ip_category = IPCategory.LPW

    def PHY_Sidewalk_2GFSK_50Kbps(self, model):
        phy = self._makePhy(model, model.profiles.sidewalk, readable_name='Sidewalk 50kbps 2GFSK mi=1.0')

        phy.profile_inputs.sidewalk_mode.value = model.vars.sidewalk_mode.var_enum.FSK_50kbps
        phy.profile_inputs.preamble_length.value = 8 * 8  # Use 64-bit preamble for TX by default
        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.Gaussian
        phy.profile_inputs.shaping_filter_param.value = 1.0

        phy.profile_inputs.base_frequency_hz.value = 902200000
        phy.profile_inputs.channel_spacing_hz.value = 200000

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)

        return phy


    '''
    def PHY_Sidewalk_2GFSK_150Kbps(self, model):
        phy = self._makePhy(model, model.profiles.sidewalk, readable_name='Sidewalk 150kbps 2GFSK mi=0.5')

        phy.profile_inputs.sidewalk_mode.value = model.vars.sidewalk_mode.var_enum.FSK_150kbps
        phy.profile_inputs.preamble_length.value = 8 * 8  # Use 64-bit preamble for TX by default
        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.Gaussian
        phy.profile_inputs.shaping_filter_param.value = 0.5

        phy.profile_inputs.base_frequency_hz.value = 902200000
        phy.profile_inputs.channel_spacing_hz.value = 406349

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)

    def PHY_Sidewalk_2GFSK_250Kbps(self, model):
        phy = self._makePhy(model, model.profiles.sidewalk, readable_name='Sidewalk 250kbps 2GFSK mi=0.5')

        phy.profile_inputs.sidewalk_mode.value = model.vars.sidewalk_mode.var_enum.FSK_250kbps
        phy.profile_inputs.preamble_length.value = 8 * 8  # Use 64-bit preamble for TX by default
        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.Gaussian
        phy.profile_inputs.shaping_filter_param.value = 0.5

        phy.profile_inputs.base_frequency_hz.value = 902200000
        phy.profile_inputs.channel_spacing_hz.value = 512000

        self._set_xtal_frequency(phy, xtal_freq_hz=39000000)
    '''