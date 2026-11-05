
class XtalFrequencyMixin:
    default_xtal_frequency_hz = None

    def _set_xtal_frequency(self, phy, xtal_freq_hz=None):
        if xtal_freq_hz is None:
            xtal_freq_hz = self.default_xtal_frequency_hz

        if xtal_freq_hz is None:
            raise ValueError("default_xtal_frequency_hz must be set by the PHY class")

        phy.profile_inputs.xtal_frequency_hz.value = xtal_freq_hz
        