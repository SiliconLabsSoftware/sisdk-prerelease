"""
System Feature PHYs to expose in GSDK.
"""
from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pyradioconfig.parts.sol.phys.Phys_Studio_Base_Standard_SUNFSK import PHYS_Studio_Base_Standard_SUNFSK_Sol
from pyradioconfig.parts.rainier.phys.Phys_RAIL_Base_Standard_IEEE802154 import PhysRailBaseStandardIeee802154Rainier
from pyradioconfig.parts.rainier.phys._Phys_common import fast_detection_agc_settings
from py_2_and_3_compatibility import *
from pyradioconfig.calculator_model_framework.decorators.phy_decorators import concurrent_phy
from pycalcmodel.core.common import IPCategory


class PhysRailBaseSystemFeaturesRainier(IPhy):

    ip_category = IPCategory.LPW

    default_xtal_frequency_hz = 38400000

    def _part_specific_phy_overrides(self, phy, model):
        pass

    def _get_rail_base_802154_class(self):
        return PhysRailBaseStandardIeee802154Rainier()

    def PHY_IEEE802154_2p4GHz_2ChannelFS(self, model, phy_name=None):
        """Meant to be both RX and TX capable"""
        phy = self._get_rail_base_802154_class().PHY_IEEE802154_2p4GHz_Enhanced_Scan(model, phy_name)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.FCS
        return phy

    def PHY_IEEE802154_2p4GHz_DutyCycle(self, model, phy_name=None):
        """802154 duty cycling PHY using the noise detector"""
        phy = self._get_rail_base_802154_class().PHY_IEEE802154_2p4GHz_Enhanced_DutyCycling(model, phy_name)
        return phy

    def PHY_IEEE802154_2p4GHz_2ChannelFS_DutyCycle(self, model, phy_name=None):
        """PHY was functionally verified to be able to receive on 2 channels (2ZB)"""
        phy = self._get_rail_base_802154_class().PHY_IEEE802154_2p4GHz_Enhanced_DutyCycling(model, phy_name)
        self._get_rail_base_802154_class().fast_hopping_demod_ctrl_settings(phy, model)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.NONE    ## 2ChannelFS with RXDC not supported by RAIL yet
        phy.profile_inputs.rxdc_power_save_time_us.value = 5
        phy.profile_inputs.agc_power_mode.value = model.vars.agc_power_mode.var_enum.LP     # 100uA before DC
        phy.profile_inputs.rfpkd_mode.value = model.vars.rfpkd_mode.var_enum.DISABLE
        phy.profile_outputs.MODEM_SICTRL0_NOISETHRESH.override = 140        # Faster noise detector, lower PER floor by trading sensitivity
        phy.profile_outputs.MODEM_SRCCHF_CHMUTETIMER.override = 230
        phy.profile_outputs.MODEM_EHDSSSCFG2_DSSSFRTCORRTHD.override = 650      # Freq offset issues if set to same as 2ZB
        return phy


    def _HDRModeSwitch_comm_settings(self, phy, model):
        PHYS_Studio_Base_Standard_SUNFSK_Sol().SUN_FSK_base(phy, model)

        phy.profile_inputs.syncword_0.value = 0
        phy.profile_inputs.syncword_1.value = 0

        phy.profile_inputs.modulation_type.value = model.vars.modulation_type.var_enum.FSK2
        phy.profile_inputs.deviation.value = 500000
        phy.profile_inputs.channel_spacing_hz.value = 3000000  # Don't care

        phy.profile_inputs.base_frequency_hz.value = long(2456230005)
        phy.profile_inputs.if_frequency_hz.value = 1370000
        self._set_xtal_frequency(phy)
        phy.profile_inputs.rx_xtal_error_ppm.value = 10  # TBD, but probably doesn't matter as we are at max BW
        phy.profile_inputs.tx_xtal_error_ppm.value = 10  # TBD, but probably doesn't matter as we are at max BW

        phy.profile_inputs.bandwidth_hz.value = 2200000
        phy.profile_inputs.baudrate_tol_ppm.value = 40

        phy.profile_inputs.preamble_length.value = 16
        phy.profile_inputs.syncword_length.value = 32

        phy.profile_inputs.shaping_filter_param.value = 0.5

        phy.profile_inputs.payload_white_en.value = True

        fast_detection_agc_settings(phy, model)

        phy.profile_outputs.AGC_STEPDWN_STEPDWN0.override = 0
        phy.profile_outputs.AGC_STEPDWN_STEPDWN1.override = 1
        phy.profile_outputs.AGC_STEPDWN_STEPDWN2.override = 2
        phy.profile_outputs.AGC_STEPDWN_STEPDWN3.override = 3
        phy.profile_outputs.AGC_STEPDWN_STEPDWN4.override = 3
        phy.profile_outputs.AGC_STEPDWN_STEPDWN5.override = 5
        phy.profile_outputs.AGC_AGCPERIOD0_PERIODHI.override = 14
        phy.profile_outputs.AGC_AGCPERIOD1_PERIODLOW.override = 70

        self._part_specific_phy_overrides(phy, model)
        pass

    def _HDRModeSwitch_dualsync_settings(self, phy, model):
        phy.profile_inputs.syncword_1.value = 0xF4
        self._get_rail_base_802154_class().fast_framedet_ehdsss_settings(phy, model)
        self._set_xtal_frequency(phy)
        pass

    def _154OQPSK_dualsync_HDRModeSwitch_1M_minimal_diff(self, phy, model):

        phy.profile_outputs.MODEM_AFC_AFCLIMRESET.override = 1
        phy.profile_outputs.MODEM_AFCADJRX_AFCSCALEE.override = 5
        phy.profile_outputs.MODEM_AFCADJRX_AFCSCALEM.override = 21
        phy.profile_outputs.MODEM_CGCLKSTOP_FORCEOFF.override = 65023 #TODO: Check if this is needed
        phy.profile_outputs.MODEM_CHFCTRL_SWCOEFFEN.override = 0
        phy.profile_outputs.MODEM_CTRL0_CODING.override = 0
        phy.profile_outputs.MODEM_CTRL0_MODFORMAT.override = 0
        phy.profile_outputs.MODEM_CTRL1_SYNCBITS.override = 31
        phy.profile_outputs.MODEM_EHDSSSCTRL_EHDSSSEN.override = 0
        phy.profile_outputs.MODEM_FRMSCHTIME_DSARSTSYCNEN.override = 0
        phy.profile_outputs.MODEM_FRMSCHTIME_FRMSCHTIME.override = 64
        phy.profile_outputs.MODEM_MODINDEX_FREQGAINE.override = 2
        phy.profile_outputs.MODEM_MODINDEX_FREQGAINM.override = 7
        phy.profile_outputs.MODEM_MODINDEX_MODINDEXE.override = 26
        phy.profile_outputs.MODEM_MODINDEX_MODINDEXM.override = 215
        phy.profile_outputs.MODEM_PHDMODCTRL_CHPWRQUAL.override = 0
        phy.profile_outputs.MODEM_PHDMODCTRL_PMDETEN.override = 0
        phy.profile_outputs.MODEM_PRE_BASE.override = 2
        phy.profile_outputs.MODEM_PRE_BASEBITS.override = 1
        phy.profile_outputs.MODEM_SYNC0_SYNC0.override = 1418178947
        phy.profile_outputs.MODEM_SYNCWORDCTRL_DUALSYNC.override = 0
        phy.profile_outputs.MODEM_VITERBIDEMOD_VTDEMODEN.override = 1

        phy.profile_outputs.FRC_DFLCTRL_DFLBITORDER.override = 1
        phy.profile_outputs.FRC_DFLCTRL_DFLBITS.override = 11
        phy.profile_outputs.FRC_DFLCTRL_DFLMODE.override = 4
        phy.profile_outputs.FRC_DFLCTRL_DFLOFFSET.override = 1
        phy.profile_outputs.FRC_DFLCTRL_MINLENGTH.override = 1
        phy.profile_outputs.FRC_FCD0_WORDS.override = 1
        phy.profile_outputs.FRC_FCD1_SKIPWHITE.override = 0
        phy.profile_outputs.FRC_FCD2_WORDS.override = 1
        phy.profile_outputs.FRC_FCD3_SKIPWHITE.override = 0
        phy.profile_outputs.FRC_FECCTRL_BLOCKWHITEMODE.override = 4
        phy.profile_outputs.FRC_FECCTRL_CONVMODE.override = 1
        phy.profile_outputs.FRC_FECCTRL_INTERLEAVEMODE.override = 1
        phy.profile_outputs.FRC_MAXLENGTH_MAXLENGTH.override = 2048
        phy.profile_outputs.FRC_TRAILTXDATACTRL_TRAILTXDATA.override = 11
        phy.profile_outputs.FRC_WHITEINIT_WHITEINIT.override = 255
        phy.profile_outputs.RFCRC_CTRL_CRCWIDTH.override = 3
        phy.profile_outputs.RFCRC_CTRL_OUTPUTINV.override = 1
        phy.profile_outputs.RFCRC_CTRL_PADCRCINPUT.override = 1
        phy.profile_outputs.RFCRC_INIT_INIT.override = 4294967295
        phy.profile_outputs.RFCRC_POLY_POLY.override = 3988292384

        phy.profile_outputs.SEQ_MODEMINFO_ENHDSSS_EN.override = 0
        phy.profile_outputs.SEQ_MODEMINFO_TRECS_EN.override = 1
        phy.profile_outputs.SEQ_MODINDEX_CALC_FREQGAINE.override = 2
        phy.profile_outputs.SEQ_MODINDEX_CALC_FREQGAINM.override = 7
        phy.profile_outputs.SEQ_MODINDEX_CALC_MODINDEXE.override = 26
        phy.profile_outputs.SEQ_MODINDEX_CALC_MODINDEXM.override = 215
        phy.profile_outputs.SEQ_MODINDEX_CALC_MODINDEXE_DOUBLED_FREQGAINE.override = 2
        phy.profile_outputs.SEQ_MODINDEX_CALC_MODINDEXE_DOUBLED_FREQGAINM.override = 7
        phy.profile_outputs.SEQ_MODINDEX_CALC_MODINDEXE_DOUBLED_MODINDEXE.override = 27
        phy.profile_outputs.SEQ_MODINDEX_CALC_MODINDEXE_DOUBLED_MODINDEXM.override = 215
        pass

    def _HDRModeSwitch_1M_2M_minimal_diff(self, phy, model):
        phy.profile_outputs.MODEM_VTCORRCFG0_EXPECTPATT.override = 1047666389
        phy.profile_outputs.MODEM_SYNC0_SYNC0.override = 2876788348
        phy.profile_outputs.MODEM_TRECSCFG_SOFTD.override = 0

        phy.profile_outputs.FRC_FECCTRL_BLOCKWHITEMODE.override = 1
        phy.profile_outputs.FRC_FECCTRL_CONVMODE.override = 0
        phy.profile_outputs.FRC_FECCTRL_INTERLEAVEMODE.override = 0
        pass

    def _disable_dutycycle_2chfs(self, phy, model):
        phy.profile_outputs.SEQ_MODEMINFO_RXDC1CH_EN.override = 0
        phy.profile_outputs.SEQ_MODEMINFO_RXDC2CH_EN.override = 0
        pass




    def PHY_SUN_FSK_HDR_2Mbps_500kHz(self, model, phy_name='PHY_SUN_FSK_HDR_2Mbps_500kHz'):
        phy = self._makePhy(model, model.profiles.Base,
                            readable_name='HDR 2Mbps MI=0.5 PHY Based on SUN FSK', phy_name=phy_name)

        self._HDRModeSwitch_comm_settings(phy, model)

        phy.profile_inputs.syncword_0.value = long(0x3E721ED5)
        phy.profile_inputs.bitrate.value = 2000000
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.HDR_2M

        return phy


    def PHY_SUN_FSK_HDR_1Mbps_500kHz_FEC(self, model, phy_name='PHY_SUN_FSK_HDR_1Mbps_500kHz_FEC'):
        phy = self._makePhy(model, model.profiles.Base,
                            readable_name='HDR 1Mbps MI=0.5 PHY Based on SUN FSK With FEC', phy_name=phy_name)

        self._HDRModeSwitch_comm_settings(phy, model)

        phy.profile_inputs.syncword_0.value = long(0xC18DE12A)
        phy.profile_inputs.bitrate.value = 1000000

        phy.profile_inputs.fec_tx_enable.value = model.vars.fec_tx_enable.var_enum.ENABLED
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.HDR_1M_FEC
        return phy



    def PHY_154OQPSK_Enhanced_DualSync(self, model, phy_name='PHY_154OQPSK_Enhanced_DualSync'):
        phy = self._get_rail_base_802154_class().PHY_IEEE802154_2p4GHz_Enhanced(model, phy_name=phy_name)
        self._HDRModeSwitch_dualsync_settings(phy, model)
        phy.profile_outputs.MODEM_EHDSSSCFG2_DSSSFRTCORRTHD.override = 750  # Keep same to avoid regression
        return phy



    HDRModeSwitch_regs = ['MODEM_TRECSCFG_*', 'MODEM_VITERBIDEMOD_(?!VTDEMODEN)', 'MODEM_VTCORRCFG0_*',
                       'MODEM_VTCORRCFG1_*', 'MODEM_VTTRACK_*', 'MODEM_TRECPM*', 'MODEM_REALTIMCFE_*',
                       'FRC_CONVGENERATOR_*', 'FRC_PUNCTCTRL_*', 'FRC_WHITECTRL_*', 'FRC_WHITEPOLY_*',
                       'FRC_FECCTRL_(?!CONVMODE)(?!INTERLEAVEMODE)(?!BLOCKWHITEMODE)', 'MODEM_SHAPING2_*',
                       'MODEM_SHAPING3_*', 'MODEM_SHAPING4_*', 'MOD_SHAPING2_*', 'MOD_SHAPING3_*', 'MOD_SHAPING4_*',]

    @concurrent_phy(phy_name='PHY_SUN_FSK_HDR_1Mbps_500kHz_FEC', reg_field_list=HDRModeSwitch_regs)
    def PHY_HDRModeSwitch_DualSync(self, model, phy_name='PHY_HDRModeSwitch_DualSync'):
        phy = self.PHY_154OQPSK_Enhanced_DualSync(model, phy_name=phy_name)
        return phy


    HDRModeSwitch_shapingfilt = {'shaping_filter': 'Gaussian', 'shaping_filter_param':0.5}

    @concurrent_phy(phy_name='PHY_HDRModeSwitch_DualSync', reg_field_list = ['.'], override_dict = HDRModeSwitch_shapingfilt)
    def PHY_HDRModeSwitch_HDR_1M(self, model, phy_name='PHY_HDRModeSwitch_HDR_1M'):
        phy = self.PHY_SUN_FSK_HDR_1Mbps_500kHz_FEC(model, phy_name=phy_name)
        self._154OQPSK_dualsync_HDRModeSwitch_1M_minimal_diff(phy, model)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.HDR_1M_FEC
        return phy

    @concurrent_phy(phy_name='PHY_HDRModeSwitch_HDR_1M', reg_field_list=['.'])
    def PHY_HDRModeSwitch_HDR_2M(self, model, phy_name='PHY_HDRModeSwitch_HDR_2M'):
        phy = self.PHY_SUN_FSK_HDR_2Mbps_500kHz(model, phy_name=phy_name)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.HDR_2M
        self._HDRModeSwitch_1M_2M_minimal_diff(phy, model)
        return phy



    def PHY_154OQPSK_Enhanced_DutyCycle_DualSync(self, model, phy_name='PHY_154OQPSK_Enhanced_DutyCycle_DualSync'):
        phy = self.PHY_IEEE802154_2p4GHz_DutyCycle(model, phy_name=phy_name)
        self._HDRModeSwitch_dualsync_settings(phy, model)

        phy.profile_inputs.agc_power_mode.value = model.vars.agc_power_mode.var_enum.LP     # 100uA before DC
        phy.profile_inputs.rfpkd_mode.value = model.vars.rfpkd_mode.var_enum.DISABLE        # 230uA before DC
        return phy

    @concurrent_phy(phy_name='PHY_SUN_FSK_HDR_1Mbps_500kHz_FEC', reg_field_list=HDRModeSwitch_regs)
    def PHY_HDRModeSwitch_DutyCycle_DualSync(self, model, phy_name='PHY_HDRModeSwitch_DutyCycle_DualSync'):
        """Single channel, low-power 154 OQPSK ModeSwitch PHY capable of mode-switch and duty-cycling using noise detector"""
        phy = self.PHY_154OQPSK_Enhanced_DutyCycle_DualSync(model, phy_name)
        return phy

    @concurrent_phy(phy_name='PHY_HDRModeSwitch_DutyCycle_DualSync', reg_field_list = ['.'], override_dict = HDRModeSwitch_shapingfilt)
    def PHY_HDRModeSwitch_DutyCycle_HDR_1M(self, model, phy_name='PHY_HDRModeSwitch_DutyCycle_HDR_1M'):
        phy = self.PHY_SUN_FSK_HDR_1Mbps_500kHz_FEC(model, phy_name=phy_name)
        self._154OQPSK_dualsync_HDRModeSwitch_1M_minimal_diff(phy, model)
        self._disable_dutycycle_2chfs(phy, model)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.NONE
        return phy

    @concurrent_phy(phy_name='PHY_HDRModeSwitch_DutyCycle_HDR_1M', reg_field_list=['.'])
    def PHY_HDRModeSwitch_DutyCycle_HDR_2M(self, model, phy_name='PHY_HDRModeSwitch_DutyCycle_HDR_2M'):
        phy = self.PHY_SUN_FSK_HDR_2Mbps_500kHz(model, phy_name=phy_name)
        self._HDRModeSwitch_1M_2M_minimal_diff(phy, model)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.NONE
        return phy


    def PHY_154OQPSK_Enhanced_2ChannelFS_DualSync(self, model, phy_name='PHY_154OQPSK_Enhanced_2ChannelFS_DualSync'):
        phy = self.PHY_IEEE802154_2p4GHz_2ChannelFS(model, phy_name)
        self._HDRModeSwitch_dualsync_settings(phy, model)
        return phy

    @concurrent_phy(phy_name='PHY_SUN_FSK_HDR_1Mbps_500kHz_FEC', reg_field_list=HDRModeSwitch_regs)
    def PHY_HDRModeSwitch_2ChannelFS_DualSync(self, model, phy_name='PHY_HDRModeSwitch_2ChannelFS_DualSync'):
        """
        PHY is capable of dual syncword detection.
        Specifically, it can receive special "Mode Switch" SFD (syncword) in parallel with typical IEEE802154 OQPSK SFD.
        Cannot receive full packets without FW action + sequence table
        """
        phy = self.PHY_154OQPSK_Enhanced_2ChannelFS_DualSync(model, phy_name)
        return phy

    @concurrent_phy(phy_name='PHY_HDRModeSwitch_2ChannelFS_DualSync', reg_field_list = ['.'], override_dict = HDRModeSwitch_shapingfilt)
    def PHY_HDRModeSwitch_2ChannelFS_HDR_1M(self, model, phy_name='PHY_HDRModeSwitch_2ChannelFS_HDR_1M'):
        phy = self.PHY_SUN_FSK_HDR_1Mbps_500kHz_FEC(model, phy_name=phy_name)
        self._154OQPSK_dualsync_HDRModeSwitch_1M_minimal_diff(phy, model)
        self._disable_dutycycle_2chfs(phy, model)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.FCS_HDR_1M_FEC
        return phy

    @concurrent_phy(phy_name='PHY_HDRModeSwitch_2ChannelFS_HDR_1M', reg_field_list=['.'])
    def PHY_HDRModeSwitch_2ChannelFS_HDR_2M(self, model, phy_name='PHY_HDRModeSwitch_2ChannelFS_HDR_2M'):
        phy = self.PHY_SUN_FSK_HDR_2Mbps_500kHz(model, phy_name=phy_name)
        self._HDRModeSwitch_1M_2M_minimal_diff(phy, model)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.FCS_HDR_2M
        return phy

    def PHY_154OQPSK_Enhanced_2ChannelFS_DutyCycle_DualSync(self, model, phy_name='PHY_154OQPSK_Enhanced_2ChannelFS_DutyCycle_DualSync'):
        phy = self.PHY_IEEE802154_2p4GHz_2ChannelFS_DutyCycle(model, phy_name)
        self._HDRModeSwitch_dualsync_settings(phy, model)
        return phy

    @concurrent_phy(phy_name='PHY_SUN_FSK_HDR_1Mbps_500kHz_FEC', reg_field_list=HDRModeSwitch_regs)
    def PHY_HDRModeSwitch_2ChannelFS_DutyCycle_DualSync(self, model, phy_name="PHY_HDRModeSwitch_2ChannelFS_DutyCycle_DualSync"):
        """2-channel duty-cycling PHY for 2-channel mode switch"""
        phy = self.PHY_154OQPSK_Enhanced_2ChannelFS_DutyCycle_DualSync(model, phy_name)
        return phy

    @concurrent_phy(phy_name='PHY_HDRModeSwitch_2ChannelFS_DutyCycle_DualSync', reg_field_list = ['.'], override_dict = HDRModeSwitch_shapingfilt)
    def PHY_HDRModeSwitch_2ChannelFS_DutyCycle_HDR_1M(self, model, phy_name='PHY_HDRModeSwitch_2ChannelFS_DutyCycle_HDR_1M'):
        phy = self.PHY_SUN_FSK_HDR_1Mbps_500kHz_FEC(model, phy_name=phy_name)
        self._154OQPSK_dualsync_HDRModeSwitch_1M_minimal_diff(phy, model)
        self._disable_dutycycle_2chfs(phy, model)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.NONE
        return phy

    @concurrent_phy(phy_name='PHY_HDRModeSwitch_2ChannelFS_DutyCycle_HDR_1M', reg_field_list=['.'])
    def PHY_HDRModeSwitch_2ChannelFS_DutyCycle_HDR_2M(self, model, phy_name='PHY_HDRModeSwitch_2ChannelFS_DutyCycle_HDR_2M'):
        phy = self.PHY_SUN_FSK_HDR_2Mbps_500kHz(model, phy_name=phy_name)
        self._HDRModeSwitch_1M_2M_minimal_diff(phy, model)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.NONE
        return phy
