from pyradioconfig.parts.bobcat.phys.Phys_RAIL_Base_Standard_IEEE802154 import PhysRAILBaseStandardIEEE802154Bobcat
from pyradioconfig.parts.common.phys.phy_common import PHY_COMMON_FRAME_154
from pyradioconfig.parts.panther.phys.PHY_internal_base import Phy_Internal_Base
from pyradioconfig.parts.rainier.phys._Phys_common import fast_detection_agc_settings
from pyradioconfig.calculator_model_framework.decorators.phy_decorators import concurrent_phy
from py_2_and_3_compatibility import *


class PhysRailBaseStandardIeee802154Rainier(PhysRAILBaseStandardIEEE802154Bobcat):

    default_xtal_frequency_hz = 38400000

    def _part_specific_phy_overrides(self, phy, model):
        pass




    def PHY_IEEE802154_2p4GHz_iqmod(self, model, phy_name=None):
        phy = self.PHY_IEEE802154_2p4GHz(model, phy_name="PHY_IEEE802154_2p4GHz_iqmod")
        phy.profile_inputs.modulator_select.value = model.vars.modulator_select.var_enum.IQ_MOD
        return phy


    def PHY_IEEE802154_2p4GHz_cohdsa(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='802154 2p4GHz cohdsa', phy_name=phy_name)
        self.IEEE802154_2p4GHz_cohdsa_base(phy, model)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.COHERENT

        phy.profile_outputs.AGC_GAINRANGE_PNGAINSTEP.override = 3
        phy.profile_outputs.MODEM_LONGRANGE1_AVGWIN.override = 2

        return phy

    def PHY_IEEE802154_915MHz_OQPSK_coh(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='Legacy IEEE 802.15.4 2p4GHz PHY from Jumbo',
                            phy_name=phy_name)
        self.IEEE802154_2p4GHz_cohdsa_base(phy, model)
        phy.profile_outputs.MODEM_CTRL0_SHAPING.override = 2
        phy.profile_outputs.MODEM_SHAPING0_COEFF0.override = 1
        phy.profile_outputs.MODEM_SHAPING0_COEFF1.override = 1
        phy.profile_outputs.MODEM_SHAPING0_COEFF2.override = 16
        phy.profile_outputs.MODEM_SHAPING0_COEFF3.override = 48

        phy.profile_outputs.MODEM_SHAPING1_COEFF4.override = 80
        phy.profile_outputs.MODEM_SHAPING1_COEFF5.override = 112
        phy.profile_outputs.MODEM_SHAPING1_COEFF6.override = 127
        phy.profile_outputs.MODEM_SHAPING1_COEFF7.override = 127
        return phy

    def IEEE802154_2p4GHz_cohdsa_base(self, phy, model):
        """
        This base setting has been ported from Ocelot and required changes were made for series 3. Still need to
        make sure if there are any overrides in these settings that we are already calculating.
        @Shikhar has just rearranged the settings in more readable format now.
        """

        """" Basic Setting"""
        phy.profile_inputs.base_frequency_hz.value = long(2450000000)
        phy.profile_inputs.bitrate.value = 250000
        phy.profile_inputs.channel_spacing_hz.value = 5000000
        phy.profile_inputs.deviation.value = 500000

        phy.profile_inputs.bandwidth_hz.value = 2524800
        phy.profile_inputs.if_frequency_hz.value = 1370000

        phy.profile_inputs.baudrate_tol_ppm.value = 0
        phy.profile_inputs.tx_xtal_error_ppm.value = 0
        phy.profile_inputs.rx_xtal_error_ppm.value = 0

        self._set_xtal_frequency(phy)

        """ Packet Structure"""
        PHY_COMMON_FRAME_154(phy, model)
        phy.profile_inputs.var_length_minlength.value = 4
        phy.profile_inputs.syncword_0.value = long(0xe5)
        phy.profile_inputs.syncword_1.value = long(0x0)
        phy.profile_inputs.syncword_length.value = 8
        phy.profile_inputs.preamble_length.value = 32
        phy.profile_inputs.preamble_pattern.value = 0
        phy.profile_inputs.preamble_pattern_len.value = 4

        """Modulation and Symbol Encoding settings """
        phy.profile_inputs.symbol_encoding.value = model.vars.symbol_encoding.var_enum.DSSS
        phy.profile_inputs.diff_encoding_mode.value = model.vars.diff_encoding_mode.var_enum.DISABLED
        phy.profile_inputs.dsss_chipping_code.value = long(0x744AC39B)
        phy.profile_inputs.dsss_len.value = 32
        phy.profile_inputs.dsss_spreading_factor.value = 8
        phy.profile_inputs.frequency_comp_mode.value = model.vars.frequency_comp_mode.var_enum.DISABLED
        phy.profile_inputs.fsk_symbol_map.value = model.vars.fsk_symbol_map.var_enum.MAP0
        phy.profile_inputs.modulation_type.value = model.vars.modulation_type.var_enum.OQPSK

        """ Modulation settings """

        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.Custom_OQPSK
        phy.profile_inputs.shaping_filter_param.value = 0.0
        phy.profile_inputs.pll_bandwidth_tx.value = model.vars.pll_bandwidth_tx.var_enum.BW_1500KHz
        phy.profile_inputs.pll_bandwidth_rx.value = model.vars.pll_bandwidth_rx.var_enum.BW_250KHz

        """ Demod settings"""

        phy.profile_inputs.demod_select.value = model.vars.demod_select.var_enum.COHERENT
        phy.profile_inputs.target_osr.value = 5  # Calc SRC

        phy.profile_outputs.MODEM_CTRL1_PHASEDEMOD.override = 2
        phy.profile_outputs.MODEM_CTRL2_DATAFILTER.override = 7

        Phy_Internal_Base.AGC_FAST_LOOP_base(phy, model)
        phy.profile_inputs.rssi_period.value = 3
        phy.profile_outputs.AGC_CTRL0_PWRTARGET.override = 20
        phy.profile_outputs.AGC_CTRL1_PWRPERIOD.override = 4

        phy.profile_outputs.MODEM_DIGIGAINCTRL_DIGIGAINDOUBLE.override = 0
        phy.profile_outputs.MODEM_DIGIGAINCTRL_DIGIGAINEN.override = 1
        phy.profile_outputs.MODEM_DIGIGAINCTRL_DIGIGAINSEL.override = 20  # 20

        phy.profile_outputs.MODEM_LONGRANGE1_AVGWIN.override = 4
        phy.profile_outputs.MODEM_LONGRANGE1_HYSVAL.override = 3
        phy.profile_outputs.MODEM_COH0_COHDYNAMICBBSSEN.override = 1

        LRCHPWR_sens = 32 # target sens is at -105.5 which is between CW CHPWR 30 and 31

        phy.profile_outputs.MODEM_LONGRANGE2_LRCHPWRTH1.override = LRCHPWR_sens - 6 - 6
        phy.profile_outputs.MODEM_LONGRANGE2_LRCHPWRTH2.override = LRCHPWR_sens - 6 # this is neede dfo cases with strong blocker
        phy.profile_outputs.MODEM_LONGRANGE2_LRCHPWRTH3.override = LRCHPWR_sens # use BBSS 4 at for best sens
        phy.profile_outputs.MODEM_LONGRANGE2_LRCHPWRTH4.override = LRCHPWR_sens + 6
        phy.profile_outputs.MODEM_LONGRANGE3_LRCHPWRTH5.override = LRCHPWR_sens + 6 + 6
        phy.profile_outputs.MODEM_LONGRANGE3_LRCHPWRTH6.override = LRCHPWR_sens + 6 + 6 + 6
        phy.profile_outputs.MODEM_LONGRANGE3_LRCHPWRTH7.override = LRCHPWR_sens + 6 + 6 + 6 + 6
        phy.profile_outputs.MODEM_LONGRANGE3_LRCHPWRTH8.override = LRCHPWR_sens + 6 + 6 + 6 + 6 + 6
        phy.profile_outputs.MODEM_LONGRANGE4_LRCHPWRTH9.override = LRCHPWR_sens + 6 + 6 + 6 + 6 + 6 + 6
        phy.profile_outputs.MODEM_LONGRANGE4_LRCHPWRTH10.override =LRCHPWR_sens + 6 + 6 + 6 + 6 + 6 + 6 + 6
        phy.profile_outputs.MODEM_LONGRANGE6_LRCHPWRTH11.override = 0 # unused

        phy.profile_outputs.MODEM_LONGRANGE4_LRCHPWRSH1.override = 2
        phy.profile_outputs.MODEM_LONGRANGE4_LRCHPWRSH2.override = 3
        phy.profile_outputs.MODEM_LONGRANGE4_LRCHPWRSH3.override = 4
        phy.profile_outputs.MODEM_LONGRANGE4_LRCHPWRSH4.override = 5
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH5.override = 6
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH6.override = 7
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH7.override = 8
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH8.override = 9
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH9.override = 10
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH10.override = 11
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH11.override = 12
        phy.profile_outputs.MODEM_LONGRANGE6_LRCHPWRSH12.override = 0 # unused

        LRSPIKETHD = 130 # allowing some false detections to perform better near sensitivity
        FIXEDCDTHFORIIR = 145 # # choosing a value of LRSPIKETHD that causes no false detections at static BBSS in 10s
        LRCHPWRSPIKETH = 34  # use fixed DSA threshold till -100dBm

        phy.profile_outputs.MODEM_COH3_COHDSAEN.override = 1
        phy.profile_outputs.MODEM_COH3_CDSS.override = 4
        phy.profile_outputs.MODEM_COH3_COHDSAADDWNDSIZE.override = 80
        phy.profile_outputs.MODEM_COH2_FIXEDCDTHFORIIR.override = FIXEDCDTHFORIIR

        phy.profile_outputs.MODEM_COH2_DSAPEAKCHPWRTH.override = 200
        phy.profile_outputs.MODEM_COH3_PEAKCHKTIMOUT.override = 18

        phy.profile_outputs.MODEM_LONGRANGE1_PREFILTLEN.override = 3  # LEN128
        phy.profile_outputs.MODEM_COH3_DYNIIRCOEFOPTION.override = 3
        phy.profile_outputs.MODEM_COH3_DSAPEAKINDLEN.override = 4

        phy.profile_outputs.MODEM_LONGRANGE6_LRCHPWRSPIKETH.override = LRCHPWRSPIKETH

        phy.profile_outputs.MODEM_LONGRANGE6_LRSPIKETHD.override = LRSPIKETHD

        phy.profile_inputs.timing_detection_threshold.value = 65
        phy.profile_inputs.timing_sample_threshold.value = 0
        phy.profile_inputs.number_of_timing_windows.value = 7
        phy.profile_inputs.symbols_in_timing_window.value = 12
        phy.profile_outputs.MODEM_TIMING_TIMTHRESH.override = 35
        phy.profile_outputs.MODEM_CTRL6_TIMTHRESHGAIN.override = 2

        phy.profile_outputs.MODEM_CTRL6_TDREW.override = 64 # symbol rewind
        phy.profile_outputs.MODEM_CTRL5_LINCORR.override = 1 # enable check that correlation peak is approx. ½ final value at halfway pt

        phy.profile_outputs.MODEM_CTRL5_FOEPREAVG.override = 7
        phy.profile_outputs.MODEM_INTAFC_FOEPREAVG0.override = 1
        phy.profile_outputs.MODEM_INTAFC_FOEPREAVG1.override = 3
        phy.profile_outputs.MODEM_INTAFC_FOEPREAVG2.override = 5
        phy.profile_outputs.MODEM_INTAFC_FOEPREAVG3.override = 5
        phy.profile_outputs.MODEM_INTAFC_FOEPREAVG4.override = 0
        phy.profile_outputs.MODEM_INTAFC_FOEPREAVG5.override = 0
        phy.profile_outputs.MODEM_INTAFC_FOEPREAVG6.override = 0
        phy.profile_outputs.MODEM_INTAFC_FOEPREAVG7.override = 0

        phy.profile_outputs.MODEM_CTRL6_PSTIMABORT0.override = 1
        phy.profile_outputs.MODEM_CTRL6_PSTIMABORT1.override = 1
        phy.profile_outputs.MODEM_CTRL6_PSTIMABORT2.override = 1
        phy.profile_outputs.MODEM_CTRL6_PSTIMABORT3.override = 0

        phy.profile_outputs.MODEM_CTRL5_DSSSCTD.override = 1  # Disable timing threshold after PREDET
        phy.profile_outputs.MODEM_CTRL6_DSSS3SYMBOLSYNCEN.override = 1  # Use 3 symbols (include 1 Preamble symbol)
        phy.profile_outputs.MODEM_CTRL5_POEPER.override = 1  # Use 1 symbol for phase estimation

        phy.profile_outputs.MODEM_COH0_COHDYNAMICPRETHRESH.override = 1
        phy.profile_outputs.MODEM_COH0_COHDYNAMICSYNCTHRESH.override = 1

        SYNCTHRESH0 = 45
        phy.profile_outputs.MODEM_COH1_SYNCTHRESH0.override = SYNCTHRESH0
        phy.profile_outputs.MODEM_COH1_SYNCTHRESH1.override = SYNCTHRESH0 + 5
        phy.profile_outputs.MODEM_COH1_SYNCTHRESH2.override = SYNCTHRESH0 + 5 + 5
        phy.profile_outputs.MODEM_COH1_SYNCTHRESH3.override = 0 # not sure why?

        phy.profile_outputs.MODEM_COH0_COHCHPWRTH0.override = LRCHPWR_sens # LRCHPWRTH3 (channel pwr at sens)
        phy.profile_outputs.MODEM_COH0_COHCHPWRTH1.override = LRCHPWR_sens + 6 # LRCHPWRTH4
        phy.profile_outputs.MODEM_COH0_COHCHPWRTH2.override = LRCHPWR_sens + 42 # LRCHPWRTH10

        phy.profile_outputs.MODEM_COH2_SYNCTHRESHDELTA0.override = 0
        phy.profile_outputs.MODEM_COH2_SYNCTHRESHDELTA1.override = 2
        phy.profile_outputs.MODEM_COH2_SYNCTHRESHDELTA2.override = 4
        phy.profile_outputs.MODEM_COH2_SYNCTHRESHDELTA3.override = 0

        """ Miscellaneous Settings """

        phy.profile_outputs.FRC_AUTOCG_AUTOCGEN.override = 7
        phy.profile_outputs.MODEM_AFC_AFCRXCLR.override = 1
        phy.profile_outputs.MODEM_AFCADJLIM_AFCADJLIM.override = 2750
        phy.profile_outputs.MODEM_CGCLKSTOP_FORCEOFF.override = 0x1E00  # 9,10,11,12
        phy.profile_outputs.MODEM_COH3_LOGICBASEDCOHDEMODGATE.override = 1
        phy.profile_outputs.MODEM_CTRL2_SQITHRESH.override = 200
        phy.profile_outputs.MODEM_CTRL6_ARW.override = 1
        phy.profile_outputs.MODEM_CTRL6_RXBRCALCDIS.override = 1
        phy.profile_outputs.MODEM_LONGRANGE1_LRTIMEOUTTHD.override = 320

        phy.profile_outputs.MODEM_CTRL2_SQITHRESH.override = 56
        model.vars.synth_tx_mode.value_forced = model.vars.synth_tx_mode.var_enum.MODE_IEEE802154

        self._part_specific_phy_overrides(phy, model)

    def PHY_IEEE802154_2p4GHz_cohdsa_iqmod(self, model, phy_name=None):
        phy = self.PHY_IEEE802154_2p4GHz_cohdsa(model, phy_name="PHY_IEEE802154_2p4GHz_cohdsa_iqmod")
        phy.profile_inputs.modulator_select.value = model.vars.modulator_select.var_enum.IQ_MOD
        return phy



    def PHY_IEEE802154_2p4GHz_Enhanced(self, model, phy_name='PHY_IEEE802154_2p4GHz_Enhanced'):
        phy = self._makePhy(model, model.profiles.Base, readable_name='IEEE 802.15.4 2p4GHz Enhanced Demod PHY',
                            phy_name=phy_name)
        self.IEEE802154_2p4GHz_base(phy, model)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.ENHANCED

        phy.profile_inputs.demod_select.value = model.vars.demod_select.var_enum.ENHANCED_DSSS

        phy.profile_inputs.base_frequency_hz.value = long(2494630022)
        phy.profile_inputs.bandwidth_hz.value = 2200000
        phy.profile_inputs.baudrate_tol_ppm.value = 40
        phy.profile_inputs.rx_xtal_error_ppm.value = 10
        phy.profile_inputs.tx_xtal_error_ppm.value = 10

        phy.profile_outputs.AGC_CTRL1_RSSIPERIOD.override = None
        phy.profile_inputs.rssi_period.value = 3

        phy.profile_outputs.MODEM_EHDSSSCTRL_DSSSFRMTIMEOUT.override = 4
        phy.profile_outputs.MODEM_EHDSSSCTRL_DSSSDSATHD.override = 3
        phy.profile_outputs.MODEM_EHDSSSCTRL_DSSSFRQLIM.override = 50
        phy.profile_outputs.MODEM_EHDSSSCTRL_FREQCORREN.override = 3
        phy.profile_outputs.MODEM_EHDSSSCTRL_FOEBIAS.override = 1
        phy.profile_outputs.MODEM_EHDSSSCTRL_DSSSTIMEACQUTHD.override = 16

        phy.profile_outputs.MODEM_EHDSSSCFG1_DSSSDSAQTHD.override = 500
        phy.profile_outputs.MODEM_EHDSSSCFG1_DSSSCORRTHD.override = 450
        phy.profile_outputs.MODEM_EHDSSSCFG1_DSSSEXPSYNCLEN.override = 128

        phy.profile_outputs.MODEM_EHDSSSCFG2_DSSSCORRSCHWIN.override = 6
        phy.profile_outputs.MODEM_EHDSSSCFG2_DSSSTRACKINGWIN.override = 5
        phy.profile_outputs.MODEM_EHDSSSCFG2_DSSSTIMCORRTHD.override = 500

        phy.profile_outputs.MODEM_EHDSSSCFG3_LQIAVGWIN.override = 1
        phy.profile_outputs.MODEM_EHDSSSCFG3_DSSSINITIMLEN.override = 3
        phy.profile_outputs.MODEM_EHDSSSCFG3_DSSSFOETRACKGEAR.override = 0
        phy.profile_outputs.MODEM_EHDSSSCFG3_DSSSDASMAXTHD.override = 1400

        phy.profile_outputs.MODEM_PHDMODCTRL_PMDETEN.override = 1
        phy.profile_outputs.MODEM_PHDMODCTRL_PMDETTHD.override = 3
        phy.profile_outputs.MODEM_PHDMODCTRL_CHPWRQUAL.override = 1
        phy.profile_outputs.MODEM_LONGRANGE1_AVGWIN.override = 3
        phy.profile_outputs.MODEM_COCURRMODE_CORRCHKMUTE.override = 8

        phy.profile_outputs.MODEM_COH0_COHCHPWRTH0.override = 214
        phy.profile_outputs.MODEM_COH0_COHCHPWRTH1.override = 64
        phy.profile_outputs.MODEM_COH0_COHCHPWRTH2.override = 8

        phy.profile_outputs.AGC_CTRL7_SUBPERIOD.override = 1
        phy.profile_outputs.AGC_CTRL7_SUBINT.override = 8
        phy.profile_outputs.AGC_CTRL7_SUBDEN.override = 2

        phy.profile_outputs.MODEM_FRMSCHTIME_DSARSTSYCNEN.override = 1
        phy.profile_outputs.MODEM_FRMSCHTIME_FRMSCHTIME.override = 40

        phy.profile_outputs.MODEM_CGCLKSTOP_FORCEOFF.override = 56831
        return phy



    def PHY_IEEE802154_2p4GHz_Enhanced_Scan(self, model, phy_name='PHY_IEEE802154_2p4GHz_Enhanced_Scan'):
        phy = self.PHY_IEEE802154_2p4GHz_Enhanced(model, phy_name=phy_name)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.FCS

        self.fast_detection_ehdsss_settings(phy, model)
        self.fast_framedet_ehdsss_settings(phy, model)
        self.fast_hopping_demod_ctrl_settings(phy, model)

        return phy


    def fast_detection_ehdsss_settings(self, phy, model):
        phy.profile_inputs.base_frequency_hz.value = long(2440e6)

        phy.profile_outputs.MODEM_EHDSSSCTRL_DSSSDSATHD.override = 0
        phy.profile_outputs.MODEM_EHDSSSCTRL_DSSSFRQLIM.override = 50

        phy.profile_outputs.MODEM_EHDSSSCFG1_DSSSDSAQTHD.override = 700
        phy.profile_outputs.MODEM_EHDSSSCFG1_DSSSCORRTHD.override = 400
        phy.profile_outputs.MODEM_EHDSSSCFG2_DSSSTRACKINGWIN.override = 5
        phy.profile_outputs.MODEM_EHDSSSCFG3_DSSSFOETRACKGEAR.override = 0

        phy.profile_outputs.MODEM_PHDMODCTRL_PMDETEN.override = 0

        phy.profile_outputs.MODEM_COH0_COHCHPWRTH0.override = 212

        phy.profile_outputs.MODEM_AUTOCG_AUTOCGEN.override = 1
        phy.profile_outputs.MODEM_CGCLKSTOP_FORCEOFF.override = 4099

        fast_detection_agc_settings(phy, model)

        phy.profile_outputs.MODEM_AFCADJRX_AFCSCALEE.override = 3
        phy.profile_outputs.MODEM_AFCADJRX_AFCSCALEM.override = 27

    def fast_framedet_ehdsss_settings(self, phy, model):
        phy.profile_outputs.MODEM_EHDSSSCFG2_DSSSCORRSCHWIN.override = 2
        phy.profile_outputs.MODEM_EHDSSSCFG1_DSSSCORRTHD.override = 400
        phy.profile_outputs.MODEM_EHDSSSCFG3_LQIAVGWIN.override = 0

        phy.profile_outputs.MODEM_EHDSSSCTRL_DSSSDSATHD.override = 0
        phy.profile_outputs.MODEM_EHDSSSCTRL_DSSSFRMTIMEOUT.override = 7
        phy.profile_outputs.MODEM_PHDMODCTRL_PMDETEN.override = 0
        phy.profile_outputs.MODEM_EHDSSSCFG1_DSSSDSAQTHD.override = 700
        phy.profile_outputs.MODEM_EHDSSSCFG1_DSSSCORRTHD.override = 370
        phy.profile_outputs.MODEM_EHDSSSCFG2_DSSSCORRSCHWIN.override = 1

    def fast_hopping_demod_ctrl_settings(self, phy, model):
        phy.profile_inputs.hop_enable.value = model.vars.hop_enable.var_enum.ENABLED
        phy.profile_outputs.MODEM_DIGMIXCTRL_BLEORZB.override = 1  # ZB

        phy.profile_outputs.MODEM_COCURRMODE_DSSSDSACHK.override = 75           # Increase in-channel time from 30us to 45us to help with sensitivity
        phy.profile_outputs.MODEM_SQ_SQTIMOUT.override = 96                     # Set to same for all hopping PHYs
        



    ble2zb_blelist = ['MODEM_TRECSCFG_(?!DTIMLOSS)', 'MODEM_VITERBIDEMOD_*', 'MODEM_VTCORRCFG0_*', 'MODEM_VTCORRCFG1_*']

    @concurrent_phy(phy_name='PHY_Bluetooth_LE_Viterbi_noDSA_fullrate', reg_field_list=ble2zb_blelist)
    def PHY_BLE_ZB_Concurrent_Hop(self, model, phy_name='PHY_BLE_ZB_Concurrent_Hop'):
        phy = self.PHY_IEEE802154_2p4GHz_Enhanced_Scan(model, phy_name=phy_name)

        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.FCS
        model.vars.ble_feature.value_forced = model.vars.ble_feature.var_enum.FASTSW

        phy.profile_inputs.bandwidth_hz.value = 2500000
        phy.profile_inputs.target_bwsel.value = 0.23  # Chosen to deliver DEC0=4 and DEC1=2

        phy.profile_inputs.syncword_0.value = 0xE8E
        phy.profile_inputs.syncword_length.value = 12
        phy.profile_outputs.MODEM_SYNC2_SYNC2.override = 167
        phy.profile_outputs.MODEM_SYNCWORDCTRL_SYNCDET2TH.override = 1
        phy.profile_outputs.MODEM_SYNCWORDCTRL_SYNCBITS2TH.override = 7

        phy.profile_inputs.symbols_in_timing_window.value = 0

        phy.profile_outputs.MODEM_COCURRMODE_DSSSCONCURRENT.override = 1
        phy.profile_outputs.MODEM_COCURRMODE_CORRCHKMUTE.override = 8
        phy.profile_outputs.MODEM_COCURRMODE_DSSSDSACHK.override = 51
        phy.profile_outputs.MODEM_COCURRMODE_TRECSDSACHK.override = 4       # need for BLERX HOPPINGSRC=1

        phy.profile_outputs.MODEM_CTRL0_MODFORMAT.override = 0
        phy.profile_outputs.MODEM_CTRL0_CODING.override = 0

        phy.profile_outputs.MODEM_DIGMIXCTRL_DSSSCFECOMBO.override = 2
        phy.profile_outputs.MODEM_TRECPMDET_PMMINCOSTTHD.override = 650 # Improves ZB sensitivity by 1dB
        phy.profile_outputs.MODEM_TRECPMDET_PMACQUINGWIN.override = 31 # Fixes ZB Frequency Tolerance
        phy.profile_outputs.MODEM_TRECPMDET_PMTIMEOUTSEL.override = 2  # Fixes ZB Frequency Tolerance
        phy.profile_outputs.MODEM_TRECPMPATT_PMEXPECTPATT.override = 3765939820 # Fixes ZB Frequency Tolerance

        phy.profile_outputs.MODEM_DUALTIM_DUALTIMEN.override = 1
        phy.profile_outputs.MODEM_VTCORRCFG0_EXPECTPATT.override = 3906994176
        phy.profile_outputs.MODEM_VTCORRCFG1_EXPSYNCLEN.override = 74
        phy.profile_outputs.MODEM_REALTIMCFE_MINCOSTTHD.override = 225
        phy.profile_outputs.MODEM_REALTIMCFE_RTSCHWIN.override = 4
        phy.profile_outputs.MODEM_DUALTIM_SYNCACQWIN2.override = 15
        phy.profile_outputs.MODEM_DUALTIM_MINCOSTTHD2.override = 250
        phy.profile_outputs.MODEM_EXPECTPATTDUAL_EXPECTPATTDUAL.override = 0x6E8E0000

        self.IEEE802154_2p4GHz_signal_identifier_regs(phy, model)
        self.IEEE802154_2p4GHz_signal_identifier_fast_detection_overrides(phy, model)
        phy.profile_outputs.MODEM_SICTRL0_SYMIDENTDIS.override = 1
        phy.profile_outputs.MODEM_SICTRL0_PEAKNUMTHRESHLW.override = 10
        phy.profile_outputs.MODEM_SICTRL2_PEAKNUMADJEN.override = 51
        phy.profile_outputs.MODEM_SICTRL2_SISTARTDELAY.override = 1
        phy.profile_outputs.MODEM_SICTRL2_SISTARTDELAYMODE.override = 1

        phy.profile_outputs.MODEM_COH0_COHCHPWRTH0.override = 216
        phy.profile_outputs.MODEM_COH0_COHCHPWRTH1.override = 0

        phy.profile_outputs.MODEM_SQ_SQTIMOUT.override = 96
        phy.profile_outputs.MODEM_SQEXT_SQSTG2TIMOUT.override = 256
        phy.profile_outputs.MODEM_SQEXT_SQSTG3TIMOUT.override = 0

        phy.profile_outputs.MODEM_SRCCHF_CHMUTETIMER.override = 215

        return phy

    def IEEE802154_2p4GHz_signal_identifier_regs(self, phy, model):
        phy.profile_outputs.MODEM_SICTRL0_SIMODE.override = 1
        phy.profile_outputs.MODEM_SICTRL0_PEAKNUMTHRESHLW.override = 10  # 0X7
        phy.profile_outputs.MODEM_SICTRL0_FREQNOMINAL.override = 63  # 0XFF
        phy.profile_outputs.MODEM_SICTRL0_PEAKNUMADJ.override = 2  # 0X2
        phy.profile_outputs.MODEM_SICTRL0_NOISETHRESH.override = 160  # 0XA0
        phy.profile_outputs.MODEM_SICTRL0_NOISETHRESHADJ.override = 40  # 0X28
        phy.profile_outputs.MODEM_SICTRL0_SYMIDENTDIS.override = 0
        phy.profile_outputs.MODEM_SICTRL1_TWOSYMBEN.override = 1  # 0X1
        phy.profile_outputs.MODEM_SICTRL1_SMALLSAMPLETHRESH.override = 20  # 0X14
        phy.profile_outputs.MODEM_SICTRL1_SUPERCHIPTOLERANCE.override = 38  # 0X26
        phy.profile_outputs.MODEM_SICTRL1_FASTMODE.override = 1  # 0X1
        phy.profile_outputs.MODEM_SICTRL1_PEAKNUMP2ADJ.override = 3  # 0X3
        phy.profile_outputs.MODEM_SICTRL1_ZCEN.override = 0  # 0X0
        phy.profile_outputs.MODEM_SICTRL1_ZCSAMPLETHRESH.override = 8  # 0X8
        phy.profile_outputs.MODEM_SICTRL1_SOFTCLIPBYPASS.override = 0  # 0X1
        phy.profile_outputs.MODEM_SICTRL1_SOFTCLIPTHRESH.override = 100  # 0X64
        phy.profile_outputs.MODEM_SICTRL1_FREQOFFTOLERANCE.override = 0
        phy.profile_outputs.MODEM_SICTRL2_SIRSTAGCMODE.override = 1  # 0X1
        phy.profile_outputs.MODEM_SICTRL2_SIRSTPRSMODE.override = 0  # 0X0
        phy.profile_outputs.MODEM_SICTRL2_SIRSTCCAMODE.override = 0  # 0X0
        phy.profile_outputs.MODEM_SICTRL2_DISSIFRAMEDET.override = 0  # 0X0
        phy.profile_outputs.MODEM_SICTRL2_AGCRSTUPONSI.override = 0  # 0X0
        phy.profile_outputs.MODEM_SICTRL2_SUPERCHIPNUM.override = 0  # 0X0
        phy.profile_outputs.MODEM_SICTRL2_CORRNUM.override = 0  # 0X0
        phy.profile_outputs.MODEM_SICTRL2_SHFTWIN.override = 7  # 0X7
        phy.profile_outputs.MODEM_SICTRL2_NARROWPULSETHRESH.override = 31  # 0X0
        phy.profile_outputs.MODEM_SICTRL2_PEAKNUMADJEN.override = 51  # 0b00110011
        phy.profile_outputs.MODEM_SICTRL2_SISTARTDELAYMODE.override = 0
        phy.profile_outputs.MODEM_SICTRL2_SISTARTDELAY.override = 2
        phy.profile_outputs.MODEM_SICORR_CORRTHRESH.override = 306  # 0X132
        phy.profile_outputs.MODEM_SICORR_CORRTHRESHLOW.override = 56  # 0X38
        phy.profile_outputs.MODEM_SICORR_CORRTHRESHUP.override = 31  # 0X1F
        phy.profile_outputs.MODEM_SICORR_CORRTHRESH2SYMB.override = 219  # 0XDB

    def IEEE802154_2p4GHz_signal_identifier_fast_detection_overrides(self, phy, model):
        phy.profile_outputs.MODEM_SICTRL0_PEAKNUMTHRESHLW.override = 10  # 0X7
        phy.profile_outputs.MODEM_SICTRL0_SYMIDENTDIS.override = 1
        phy.profile_outputs.MODEM_SICTRL2_PEAKNUMADJEN.override = 51  # 0b00000011
        phy.profile_outputs.MODEM_SICTRL2_SISTARTDELAYMODE.override = 1
        phy.profile_outputs.MODEM_SICTRL2_SISTARTDELAY.override = 1
        phy.profile_outputs.MODEM_SICORR_CORRTHRESH.override = 306  # 0X132
        phy.profile_outputs.MODEM_SICORR_CORRTHRESHLOW.override = 56  # 0X38
        phy.profile_outputs.MODEM_SICORR_CORRTHRESHUP.override = 31  # 0X1F
        phy.profile_outputs.MODEM_SICORR_CORRTHRESH2SYMB.override = 219  # 0XDB

        if model.part_revision not in ['A0', 'A1', 'A2']:
            phy.profile_outputs.MODEM_SIFASTDETECTCTRL_FASTCWDETECTEN.override = 1
            phy.profile_outputs.MODEM_SIFASTDETECTCTRL_FASTSUPERCHIPEN.override = 0
            phy.profile_outputs.MODEM_SIFASTDETECTCTRL_FASTNARROWPULSEEN.override = 1
            phy.profile_outputs.MODEM_SIFASTDETECTCTRL_EYEOPENINGMODE.override = 2
            phy.profile_outputs.MODEM_SIFASTDETECTCTRL_CWDETECTSTARTWINDOW.override = 5
            phy.profile_outputs.MODEM_SIFASTDETECTCTRL_CWDETECTTHRESHOLD.override = 25
            phy.profile_outputs.MODEM_SIFASTDETECTCTRL_SUPERCHIPSTARTWINDOW.override = 9
            phy.profile_outputs.MODEM_SIFASTDETECTCTRL_NPULSEFIXEDTHRESHOLD.override = 1
            phy.profile_outputs.MODEM_SIFASTDETECTCTRL_NPULSETHRESHADJEN.override = 5
            phy.profile_outputs.MODEM_SIFASTDETECTCTRL_NPULSETHRESHADJ.override = 3



    def PHY_IEEE802154_2p4GHz_Enhanced_DutyCycling(self, model, phy_name='PHY_IEEE802154_2p4GHz_Enhanced_DutyCycling'):
        phy = self.PHY_IEEE802154_2p4GHz_Enhanced(model, phy_name=phy_name)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.RXDC

        self.fast_detection_ehdsss_settings(phy, model)

        self.IEEE802154_2p4GHz_signal_identifier_regs(phy, model)
        self.IEEE802154_2p4GHz_signal_identifier_fast_detection_overrides(phy, model)

        phy.profile_inputs.rxdc_power_save_mode.value = model.vars.rxdc_power_save_mode.var_enum.SYNTH
        phy.profile_inputs.rxdc_power_save_time_us.value = 23

        phy.profile_inputs.synth_settling_mode.value = model.vars.synth_settling_mode.var_enum.FAST
        phy.profile_inputs.hop_enable.value = model.vars.hop_enable.var_enum.DISABLED

        phy.profile_outputs.MODEM_SRCCHF_CHMUTETIMER.override = 170
        return phy


    def PHY_IEEE802154_2p4GHz_Enhanced_iqmod(self, model, phy_name=None):
        phy = self.PHY_IEEE802154_2p4GHz_Enhanced(model, phy_name="PHY_IEEE802154_2p4GHz_Enhanced_iqmod")
        phy.profile_inputs.modulator_select.value = model.vars.modulator_select.var_enum.IQ_MOD
        return phy

    def PHY_IEEE802154_2p4GHz_Enhanced_ANTDIV(self, model, phy_name=None):
        phy = self.PHY_IEEE802154_2p4GHz_Enhanced_Scan(model, phy_name=phy_name)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.ANTDIV
        phy.profile_inputs.hop_enable.value = model.vars.hop_enable.var_enum.DISABLED
        phy.profile_inputs.synth_settling_mode.value = model.vars.synth_settling_mode.var_enum.FAST
        phy.profile_inputs.bandwidth_hz.value = 2200000

        phy.profile_outputs.MODEM_SYNC2_SYNC2.override = 167
        phy.profile_outputs.MODEM_SYNC3_SYNC3.override = 47
        phy.profile_outputs.MODEM_SYNCWORDCTRL_SYNCDET2TH.override = 1
        phy.profile_outputs.MODEM_SYNCWORDCTRL_DUALSYNC2TH.override = 0
        phy.profile_outputs.MODEM_SYNCWORDCTRL_SYNCBITS2TH.override = 7
        phy.profile_outputs.MODEM_SYNCWORDCTRL_SYNCSWFEC.override = 0
        phy.profile_outputs.MODEM_COH0_COHCHPWRTH1.override = 0
        phy.profile_outputs.MODEM_COH0_COHCHPWRTH0.override = 216

        phy.profile_outputs.MODEM_EHDSSSCTRL_DSSSDSATHD.override = 0
        phy.profile_outputs.MODEM_EHDSSSCFG2_DSSSCORRSCHWIN.override = 6
        phy.profile_outputs.MODEM_COCURRMODE_DSSSDSACHK.override = 70
        phy.profile_outputs.MODEM_COCURRMODE_TRECSDSACHK.override = 16
        phy.profile_outputs.MODEM_COCURRMODE_CORRCHKMUTE.override = 8

        phy.profile_outputs.MODEM_DIGMIXCTRL_DSSSCFECOMBO.override = 0 ## change from 2 to 0 to save power
        phy.profile_outputs.MODEM_VTCORRCFG0_EXPECTPATT.override = 3765939820
        phy.profile_outputs.MODEM_DUALTIM_DUALTIMEN.override = 0
        phy.profile_outputs.MODEM_TRECPMDET_PMMINCOSTTHD.override = 650
        phy.profile_outputs.MODEM_REALTIMCFE_SYNCACQWIN.override = 31

        phy.profile_outputs.MODEM_CTRL3_ANTDIVMODE.override = 5
        phy.profile_outputs.MODEM_ADQUAL8_ADBAMODE.override = 1

        phy.profile_outputs.MODEM_PHDMODANTDIV_ANTWAIT.override = 66
        phy.profile_outputs.MODEM_PHDMODANTDIV_SKIP2ANT.override = 0
        phy.profile_outputs.MODEM_ADQUAL6_ADBACORRTHR .override = 250
        phy.profile_outputs.MODEM_ADQUAL8_ADBACORRTHR2.override = 500
        phy.profile_outputs.MODEM_ADQUAL6_ADBACORRDIFF.override = 50
        phy.profile_outputs.MODEM_ADQUAL7_ADBARSSITHR .override = 985
        phy.profile_outputs.MODEM_ADQUAL7_ADBARSSIDIFF.override = 6
        phy.profile_outputs.MODEM_ADQUAL4_ADAGCGRTHR  .override = 8
        phy.profile_outputs.MODEM_ADQUAL4_ADAGCGRTHR.override = 8
        phy.profile_outputs.MODEM_ADQUAL4_ADGRMODE.override = 0
        phy.profile_outputs.MODEM_ADQUAL4_ADRSSIGRTHR.override = 512
        phy.profile_outputs.MODEM_ADQUAL5_ADDIRECTCORR.override = 65535
        phy.profile_outputs.MODEM_ADQUAL6_ADBACORRDIFF.override = 50
        phy.profile_outputs.MODEM_ADQUAL6_ADBACORRTHR.override = 250
        phy.profile_outputs.MODEM_ADQUAL7_ADBARSSIDIFF.override = 0
        phy.profile_outputs.MODEM_ADQUAL7_ADBARSSITHR.override = 985
        phy.profile_outputs.MODEM_ADCTRL1_ADCTRL1.override = 1074364417
        phy.profile_outputs.MODEM_ADCTRL2_ADCTRL2.override = 1574913
        phy.profile_outputs.MODEM_ADQUAL8_ADBAAGCTHR.override = 0
        phy.profile_outputs.MODEM_ADQUAL8_ADBACORRTHR2.override = 0

        return phy

    def PHY_IEEE802154_2p4GHz_Enhanced_ANTDIV_Scan(self, model, phy_name=None):
        phy = self.PHY_IEEE802154_2p4GHz_Enhanced_ANTDIV(model, phy_name=None)
        phy.profile_inputs.hop_enable.value = model.vars.hop_enable.var_enum.ENABLED
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.NONE #ANTDIV +FCS is not yet supported

        phy.profile_outputs.MODEM_EHDSSSCTRL_DUALDSA.override = 1
        phy.profile_outputs.MODEM_DUALTIM_DUALTIMEN.override = 1
        phy.profile_outputs.MODEM_VTCORRCFG0_EXPECTPATT.override = long(0xE0770000)
        phy.profile_outputs.MODEM_REALTIMCFE_SYNCACQWIN.override = 15
        phy.profile_outputs.MODEM_REALTIMCFE_MINCOSTTHD.override = 500

        phy.profile_outputs.MODEM_EXPECTPATTDUAL_EXPECTPATTDUAL.override = long(0xAE6C0000)
        phy.profile_outputs.MODEM_DUALTIM_SYNCACQWIN2.override = 15
        phy.profile_outputs.MODEM_DUALTIM_MINCOSTTHD2.override = 500

        return phy

    def PHY_IEEE802154_2p4GHz_cohdsa_diversity(self, model, phy_name=None):
        pass

    def PHY_IEEE802154_2p4GHz_coh_interference_diversity(self, model, phy_name=None):
        pass

    def PHY_IEEE802154_2p4GHz_antdiv_fastswitch(self, model, phy_name=None):
        pass

    def _set_synth_cal_regs(self, phy):
        pass