from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pycalcmodel.core.common import IPCategory



class PHYS_Mbus_Studio_Ocelot(IPhy):

    ip_category = IPCategory.LPW

    def _part_specific_phy_overrides(self, phy, model):
        pass

    default_xtal_frequency_hz = 39000000

    def PHY_wMbus_ModeNabef_4p8K_frameA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus N, index 2/6/8/11, frameA (4.8k)', phy_name=phy_name)

        phy.profile_inputs.mbus_mode.value = model.vars.mbus_mode.var_enum.ModeN1a_4p8K

        phy.profile_inputs.preamble_length.value = 16

        phy.profile_inputs.base_frequency_hz.value = 169406250
        phy.profile_inputs.channel_spacing_hz.value = 12500

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameA
        phy.profile_inputs.mbus_symbol_encoding.value = model.vars.mbus_symbol_encoding.var_enum.NRZ
        phy.profile_inputs.syncword_dualsync.value = False

        self._set_xtal_frequency(phy)
        self._part_specific_phy_overrides(phy, model)

        return phy

    def PHY_wMbus_ModeN2g_19p2k_frameA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus N, index 4/13, frameA (19.2k)', phy_name=phy_name)

        phy.profile_inputs.mbus_mode.value = model.vars.mbus_mode.var_enum.ModeNg

        phy.profile_inputs.preamble_length.value = 16

        phy.profile_inputs.base_frequency_hz.value = 169437500
        phy.profile_inputs.channel_spacing_hz.value = 50000

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameA
        phy.profile_inputs.mbus_symbol_encoding.value = model.vars.mbus_symbol_encoding.var_enum.NRZ
        phy.profile_inputs.syncword_dualsync.value = False

        self._set_xtal_frequency(phy)
        self._part_specific_phy_overrides(phy, model)

        phy.profile_inputs.deviation_tol_ppm.value = 300000          # Required for pro2 calc to set fdev compensation

        return phy

    def PHY_wMbus_ModeT_M2O_100k_frameA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus T M2O (100k, 3of6)', phy_name=phy_name)

        phy.profile_inputs.mbus_mode.value = model.vars.mbus_mode.var_enum.ModeT_M2O_100k

        phy.profile_inputs.preamble_length.value = 38

        phy.profile_inputs.base_frequency_hz.value = 868950000
        phy.profile_inputs.channel_spacing_hz.value = 1000000

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameA
        phy.profile_inputs.mbus_symbol_encoding.value = model.vars.mbus_symbol_encoding.var_enum.MBUS_3OF6
        phy.profile_inputs.syncword_dualsync.value = False

        phy.profile_inputs.mbus_postamble_length.value = 1

        self._set_xtal_frequency(phy)
        self._part_specific_phy_overrides(phy, model)

        return phy

    def PHY_wMbus_ModeC_M2O_100k_frameA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus C M2O frameA (100k)', phy_name=phy_name)

        phy.profile_inputs.mbus_mode.value = model.vars.mbus_mode.var_enum.ModeC_M2O_100k

        phy.profile_inputs.preamble_length.value = 38

        phy.profile_inputs.base_frequency_hz.value = 868950000
        phy.profile_inputs.channel_spacing_hz.value = 1000000

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameA
        phy.profile_inputs.mbus_symbol_encoding.value = model.vars.mbus_symbol_encoding.var_enum.NRZ
        phy.profile_inputs.syncword_dualsync.value = True

        self._set_xtal_frequency(phy)
        self._part_specific_phy_overrides(phy, model)

        return phy

    def PHY_wMbus_ModeC_M2O_100k_frameB(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus C M2O frameB (100k)', phy_name=phy_name)

        self.PHY_wMbus_ModeC_M2O_100k_frameA(model)

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameB

        return phy

    def PHY_wMbus_ModeC_O2M_50k_frameA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus C O2M frameA (50k)', phy_name=phy_name)

        phy.profile_inputs.mbus_mode.value = model.vars.mbus_mode.var_enum.ModeC_O2M_50k

        phy.profile_inputs.preamble_length.value = 38

        phy.profile_inputs.base_frequency_hz.value = 869525000
        phy.profile_inputs.channel_spacing_hz.value = 1000000

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameA
        phy.profile_inputs.mbus_symbol_encoding.value = model.vars.mbus_symbol_encoding.var_enum.NRZ
        phy.profile_inputs.syncword_dualsync.value = False

        self._set_xtal_frequency(phy)
        self._part_specific_phy_overrides(phy, model)

        return phy

    def PHY_wMbus_ModeC_O2M_50k_frameB(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus C O2M frameB (50k)', phy_name=phy_name)

        self.PHY_wMbus_ModeC_O2M_50k_frameA(model)

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameB

        return phy

    def PHY_wMbus_ModeS_32p768k_frameA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus S (32.768k, Manchester)', phy_name=phy_name)

        phy.profile_inputs.mbus_mode.value = model.vars.mbus_mode.var_enum.ModeS_32p768k

        phy.profile_inputs.preamble_length.value = 30

        phy.profile_inputs.base_frequency_hz.value = 868300000
        phy.profile_inputs.channel_spacing_hz.value = 300000

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameA
        phy.profile_inputs.mbus_symbol_encoding.value = model.vars.mbus_symbol_encoding.var_enum.Manchester
        phy.profile_inputs.syncword_dualsync.value = False

        self._set_xtal_frequency(phy)
        self._part_specific_phy_overrides(phy, model)

        phy.profile_inputs.mbus_postamble_length.value = 1

        return phy

    def PHY_wMbus_ModeNabef_4p8K_frameB(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus N, index 2/6/8/11, frameB (4.8k)', phy_name=phy_name)

        self.PHY_wMbus_ModeNabef_4p8K_frameA(model)

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameB

        return phy

    def PHY_wMbus_ModeNcd_2p4K_frameA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus N, index 1/5/7/10, frameA (2.4k)', phy_name=phy_name)

        phy.profile_inputs.mbus_mode.value = model.vars.mbus_mode.var_enum.ModeN1c_2p4K

        phy.profile_inputs.preamble_length.value = 16

        phy.profile_inputs.base_frequency_hz.value = 169431250
        phy.profile_inputs.channel_spacing_hz.value = 12500

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameA
        phy.profile_inputs.mbus_symbol_encoding.value = model.vars.mbus_symbol_encoding.var_enum.NRZ
        phy.profile_inputs.syncword_dualsync.value = False

        self._set_xtal_frequency(phy)
        self._part_specific_phy_overrides(phy, model)

        return phy

    def PHY_wMbus_ModeNcd_2p4K_frameB(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus N, index 1/5/7/10, frameB (2.4k)', phy_name=phy_name)

        self.PHY_wMbus_ModeNcd_2p4K_frameA(model)

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameB

        return phy

    def PHY_wMbus_ModeN2g_19p2k_frameB(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus N, index 4/13, frameB (19.2k)', phy_name=phy_name)

        self.PHY_wMbus_ModeN2g_19p2k_frameA(model)

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameB

        return phy

    def PHY_wMbus_ModeR_4p8k_frameA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus R2 (4.8k, Manchester)', phy_name=phy_name)

        phy.profile_inputs.mbus_mode.value = model.vars.mbus_mode.var_enum.ModeR_4p8k

        phy.profile_inputs.preamble_length.value = 78

        phy.profile_inputs.base_frequency_hz.value = 868330000
        phy.profile_inputs.channel_spacing_hz.value = 60000

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameA
        phy.profile_inputs.mbus_symbol_encoding.value = model.vars.mbus_symbol_encoding.var_enum.Manchester
        phy.profile_inputs.syncword_dualsync.value = False

        self._set_xtal_frequency(phy)
        self._part_specific_phy_overrides(phy, model)

        phy.profile_inputs.mbus_postamble_length.value = 1

        return phy

    def PHY_wMbus_ModeF_2p4k_frameA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus F, frameA (2.4k)', phy_name=phy_name)

        phy.profile_inputs.mbus_mode.value = model.vars.mbus_mode.var_enum.ModeF_2p4k

        phy.profile_inputs.preamble_length.value = 78

        phy.profile_inputs.base_frequency_hz.value = 433820000
        phy.profile_inputs.channel_spacing_hz.value = 50000

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameA
        phy.profile_inputs.mbus_symbol_encoding.value = model.vars.mbus_symbol_encoding.var_enum.NRZ
        phy.profile_inputs.syncword_dualsync.value = False

        self._set_xtal_frequency(phy)
        self._part_specific_phy_overrides(phy, model)

        return phy

    def PHY_wMbus_ModeF_2p4k_frameB(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus F, frameB (2.4k)', phy_name=phy_name)

        self.PHY_wMbus_ModeF_2p4k_frameA(model)

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameB

        return phy

    def PHY_wMbus_ModeN_6p4k_frameA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus N, index 3/9/12, frameA (6.4k)', phy_name=phy_name)

        phy.profile_inputs.mbus_mode.value = model.vars.mbus_mode.var_enum.ModeN_6p4k

        phy.profile_inputs.preamble_length.value = 16

        phy.profile_inputs.base_frequency_hz.value = 169406250
        phy.profile_inputs.channel_spacing_hz.value = 12500

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameA
        phy.profile_inputs.mbus_symbol_encoding.value = model.vars.mbus_symbol_encoding.var_enum.NRZ
        phy.profile_inputs.syncword_dualsync.value = False

        self._set_xtal_frequency(phy)
        self._part_specific_phy_overrides(phy, model)

        phy.profile_inputs.deviation_tol_ppm.value = 300000          # Required for pro2 calc to set fdev compensation

        return phy

    def PHY_wMbus_ModeN_6p4k_frameB(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus N, index 3/9/12, frameB (6.4k)', phy_name=phy_name)

        self.PHY_wMbus_ModeN_6p4k_frameA(model)

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameB

        return phy

    def PHY_wMbus_ModeT_O2M_32p768k_frameA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus T O2M (32.768k, Manchester)',
                            phy_name=phy_name)

        phy.profile_inputs.mbus_mode.value = model.vars.mbus_mode.var_enum.ModeT_O2M_32p768k

        phy.profile_inputs.preamble_length.value = 30

        phy.profile_inputs.base_frequency_hz.value =  868300000
        phy.profile_inputs.channel_spacing_hz.value = 300000

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameA
        phy.profile_inputs.mbus_symbol_encoding.value = model.vars.mbus_symbol_encoding.var_enum.Manchester
        phy.profile_inputs.syncword_dualsync.value = False

        self._set_xtal_frequency(phy)
        self._part_specific_phy_overrides(phy, model)

        phy.profile_inputs.mbus_postamble_length.value = 1

        return phy


    def PHY_wMbus_ModeTC_M2O_100k_frameA(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Mbus, readable_name='WMbus TC M2O (100k, frameA)',
                            phy_name=phy_name)

        phy.profile_inputs.mbus_mode.value = model.vars.mbus_mode.var_enum.ModeTC_M2O_100k

        phy.profile_inputs.preamble_length.value = 38

        phy.profile_inputs.base_frequency_hz.value = 868950000
        phy.profile_inputs.channel_spacing_hz.value = 1000000

        phy.profile_inputs.mbus_frame_format.value = model.vars.mbus_frame_format.var_enum.FrameA
        phy.profile_inputs.mbus_symbol_encoding.value = model.vars.mbus_symbol_encoding.var_enum.MBUS_3OF6
        phy.profile_inputs.syncword_dualsync.value = False

        self._set_xtal_frequency(phy)
        self._part_specific_phy_overrides(phy, model)

        phy.profile_inputs.mbus_postamble_length.value = 1

        return phy