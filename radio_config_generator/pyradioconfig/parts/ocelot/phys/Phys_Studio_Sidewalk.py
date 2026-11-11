from pyradioconfig.parts.nerio.phys.Phys_Studio_Sidewalk import PhysStudioSidewalkNerio


class PhysStudioSidewalkOcelot(PhysStudioSidewalkNerio):
    def PHY_Sidewalk_2GFSK_50Kbps_EU(self, model):
        phy = super().PHY_Sidewalk_2GFSK_50Kbps(model)

        phy.profile_inputs.base_frequency_hz.value = 865100000
        phy.profile_inputs.channel_spacing_hz.value = 200000

        phy.profile_inputs.chcfg_base_frequency_hz.value = 865100000
        phy.profile_inputs.chcfg_channel_spacing_hz.value = 200000
        phy.profile_inputs.chcfg_channel_number_start.value = 0
        phy.profile_inputs.chcfg_channel_number_end.value = 17
        phy.profile_inputs.chcfg_physical_channel_offset.value = 0

        return phy

    def PHY_Sidewalk_2GFSK_50Kbps_ANZ(self, model):
        phy = super().PHY_Sidewalk_2GFSK_50Kbps(model)

        phy.profile_inputs.base_frequency_hz.value = 915400000
        phy.profile_inputs.channel_spacing_hz.value = 200000

        phy.profile_inputs.chcfg_base_frequency_hz.value = 915400000
        phy.profile_inputs.chcfg_channel_spacing_hz.value = 200000
        phy.profile_inputs.chcfg_channel_number_start.value = 1
        phy.profile_inputs.chcfg_channel_number_end.value = 33
        phy.profile_inputs.chcfg_physical_channel_offset.value = 1

        return phy
