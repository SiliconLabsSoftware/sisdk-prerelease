from pyradioconfig.parts.ocelot.phys.Phys_Internal_Base_Standard_IEEE802154 import PhysInternalBaseStandardIEEE802154Ocelot


class PhysRAILBaseStandardIEEE802154Bobcat(PhysInternalBaseStandardIEEE802154Ocelot):

    default_xtal_frequency_hz = 39000000

    def _part_specific_phy_overrides(self, phy, model):
        self.internal_ieee_802154_agc_overrides(phy, model)

    def IEEE802154_2p4GHz_base(self, phy, model):
        super().IEEE802154_2p4GHz_base(phy, model)

        phy.profile_inputs.base_frequency_hz.value = int(2450000000)

        phy.profile_outputs.rx_sync_delay_ns.override = 6625
        phy.profile_outputs.rx_eof_delay_ns.override = 6625

        model.vars.protocol_id.value_forced = model.vars.protocol_id.var_enum.Zigbee

    def IEEE802154_2p4GHz_cohdsa_base(self, phy, model):
        super().IEEE802154_2p4GHz_cohdsa_base(phy, model)

        phy.profile_outputs.rx_sync_delay_ns.override = 6125
        phy.profile_outputs.rx_eof_delay_ns.override = 6125

        model.vars.protocol_id.value_forced = model.vars.protocol_id.var_enum.Zigbee

    def PHY_IEEE802154_780MHz_OQPSK(self, model):
        pass

    def PHY_IEEE802154_868MHz_BPSK(self, model, phy_name=None):
        pass

    def PHY_IEEE802154_915MHz_BPSK_40kbps(self, model, phy_name=None):
        pass

    def PHY_IEEE802154_868MHz_BPSK_20kbps_coh(self, model):
        pass

    def PHY_IEEE802154_868MHz_OQPSK(self, model):
        pass


    def PHY_IEEE802154_915MHz_OQPSK(self, model):
        pass

    def PHY_IEEE802154_915MHz_OQPSK_coh(self, model):
        pass

    def PHY_IEEE802154_915MHz_BPSK_40kbps_coh(self, model):
        pass

    def PHY_IEEE802154_RSGFSK_868MHz_500kbps_mi0p76(self, model):
        pass

    def PHY_IEEE802154_2p4GHz(self, model,phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='Legacy IEEE 802.15.4 2p4GHz PHY from Jumbo',phy_name=phy_name)
        self.IEEE802154_2p4GHz_base(phy, model)
        phy.profile_inputs.demod_select.value = model.vars.demod_select.var_enum.LEGACY
        return phy

    def PHY_IEEE802154_2p4GHz_diversity_fem(self, model, phy_name=None):
        phy = self.PHY_IEEE802154_2p4GHz_diversity(model, phy_name)
        phy.profile_inputs.antdiv_adpcsigampthr.value = 704
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.ANTDIV_FEM

    def PHY_IEEE802154_2p4GHz_diversity(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='802154 2p4GHz diversity', phy_name=phy_name)
        self.IEEE802154_2p4GHz_base(phy, model)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.ANTDIV

        phy.profile_outputs.MODEM_CTRL1_PHASEDEMOD.override = 0

        phy.profile_inputs.antdiv_enable_parallel_correlation.value = True
        phy.profile_inputs.antdivmode.value = model.vars.antdivmode.var_enum.ANTSELCORR
        phy.profile_inputs.antdivrepeatdis.value = model.vars.antdivrepeatdis.var_enum.NOREPEATFIRST

        phy.profile_inputs.antdiv_switch_delay_us.value = 4.0
        phy.profile_inputs.antdiv_switch_skip_us.value = 2.0

        phy.profile_outputs.MODEM_ADPC2_ADENCORR32.override = 1
        phy.profile_outputs.MODEM_CTRL0_DUALCORROPTDIS.override = 1
        phy.profile_outputs.MODEM_ADPC2_ADPCWNDCNTRST.override = 0

        phy.profile_outputs.AGC_GAINRANGE_PNGAINSTEP.override = 3 # Change PN gain step for improved WiFi blocker performance
        phy.profile_outputs.AGC_ANTDIV_GAINMODE.override = 1
        phy.profile_outputs.AGC_GAINSTEPLIM0_CFLOOPDEL.override = 40

        phy.profile_outputs.MODEM_TIMING_TIMTHRESH.override = 90
        phy.profile_outputs.MODEM_CTRL6_TIMTHRESHGAIN.override = 1

        phy.profile_inputs.antdiv_adpcsigampthr.value = 288
        phy.profile_outputs.MODEM_CTRL5_LINCORR.override = 1
        phy.profile_outputs.MODEM_ADQUAL6_ADBACORRTHR.override = 0
        phy.profile_outputs.MODEM_ADQUAL8_ADBACORRTHR2.override = 0
        phy.profile_outputs.MODEM_ADQUAL6_ADBACORRDIFF.override = 500
        phy.profile_outputs.MODEM_ADQUAL5_ADDIRECTCORR.override = 65535

        phy.profile_outputs.MODEM_ADQUAL7_ADBARSSITHR.override = 512
        phy.profile_outputs.MODEM_ADQUAL7_ADBARSSIDIFF.override = 0
        phy.profile_outputs.MODEM_ADQUAL4_ADRSSIGRTHR.override = 512

        phy.profile_outputs.MODEM_ADPC3_ADBBSSEN.override = 0
        phy.profile_outputs.MODEM_ADPC3_ADBBSSAMPEXP.override = 11
        phy.profile_outputs.MODEM_ADPC3_ADBBSSAMPMANT.override = 0
        phy.profile_outputs.MODEM_ADPC3_ADBBSSAVGEN.override = 1
        phy.profile_outputs.MODEM_ADPC3_ADBBSSFILTLENGTH.override = 0
        phy.profile_outputs.MODEM_ADPC3_ADBBSSAVGPER.override = 2
        phy.profile_outputs.MODEM_ADPC3_ADBBSSAVGWAIT.override = 0
        phy.profile_outputs.MODEM_ADPC3_ADBBSSAVGFREEZE.override = 1

        phy.profile_outputs.MODEM_ADQUAL4_ADAGCGRTHR.override = 63
        phy.profile_outputs.MODEM_ADQUAL8_ADBAAGCTHR.override = 0
        phy.profile_outputs.MODEM_ADQUAL4_ADGRMODE.override = 0
        phy.profile_outputs.MODEM_ADQUAL8_ADBAMODE.override = 1

        phy.profile_outputs.MODEM_ADFSM0_ADSTAT1SEL.override = 6

        phy.profile_outputs.MODEM_ADPC8_ADPCOSR.override = 5
        phy.profile_outputs.MODEM_ADPC8_ADPCANTSAMPOFFSET.override = 3

        phy.profile_outputs.MODEM_CTRL6_ARW.override = 1
        phy.profile_outputs.MODEM_TIMING_ADDTIMSEQ.override = 0

        phy.profile_outputs.AGC_CTRL1_RSSIPERIOD.override = 4

        self._part_specific_phy_overrides(phy, model)

        return phy

    def PHY_IEEE802154_2p4GHz_diversity_RSSI(self, model, phy_name=None):
        phy = self.PHY_IEEE802154_2p4GHz_diversity(model, phy_name='PHY_IEEE802154_2p4GHz_diversity_RSSI')
        phy.profile_outputs.MODEM_ADQUAL8_ADBAMODE.override = 3 # : RSSI based decision only
        phy.profile_outputs.AGC_ANTDIV_DEBOUNCECNTTHD.override = 80
        phy.profile_outputs.AGC_CTRL1_RSSIPERIOD.override = 3
        return phy

    def PHY_IEEE802154_2p4GHz_cohdsa(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='802154 2p4GHz cohdsa', phy_name=phy_name)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.COHERENT
        self.IEEE802154_2p4GHz_cohdsa_base(phy, model)

        phy.profile_outputs.AGC_GAINRANGE_PNGAINSTEP.override = 3

        phy.profile_outputs.MODEM_LONGRANGE6_LRSPIKETHD.override = 115

        phy.profile_outputs.MODEM_LONGRANGE1_AVGWIN.override = 0
        phy.profile_outputs.MODEM_COH2_FIXEDCDTHFORIIR.override = 125
        phy.profile_outputs.MODEM_LONGRANGE6_LRCHPWRSPIKETH.override = 35

        self._part_specific_phy_overrides(phy, model)

        return phy

    def PHY_IEEE802154_2p4GHz_cohdsa_diversity(self, model, phy_name=None):
        pass

    def PHY_IEEE802154_2p4GHz_cohdsa_fem(self, model, phy_name=None):
        phy = self.PHY_IEEE802154_2p4GHz_cohdsa(model, phy_name)
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.FEM

        fem_chpwr_adjustment = 6

        phy.profile_outputs.MODEM_LONGRANGE2_LRCHPWRTH1.override = 20 + fem_chpwr_adjustment
        phy.profile_outputs.MODEM_LONGRANGE2_LRCHPWRTH2.override = 26 + fem_chpwr_adjustment
        phy.profile_outputs.MODEM_LONGRANGE2_LRCHPWRTH3.override = 33 + fem_chpwr_adjustment
        phy.profile_outputs.MODEM_LONGRANGE2_LRCHPWRTH4.override = 40 + fem_chpwr_adjustment
        phy.profile_outputs.MODEM_LONGRANGE3_LRCHPWRTH5.override = 46 + fem_chpwr_adjustment
        phy.profile_outputs.MODEM_LONGRANGE3_LRCHPWRTH6.override = 52 + fem_chpwr_adjustment
        phy.profile_outputs.MODEM_LONGRANGE3_LRCHPWRTH7.override = 59 + fem_chpwr_adjustment
        phy.profile_outputs.MODEM_LONGRANGE3_LRCHPWRTH8.override = 66 + fem_chpwr_adjustment
        phy.profile_outputs.MODEM_LONGRANGE4_LRCHPWRTH9.override = 73 + fem_chpwr_adjustment
        phy.profile_outputs.MODEM_LONGRANGE4_LRCHPWRTH10.override = 80 + fem_chpwr_adjustment

        phy.profile_outputs.MODEM_COH0_COHCHPWRTH0.override = 33 + fem_chpwr_adjustment
        phy.profile_outputs.MODEM_COH0_COHCHPWRTH1.override = 40 + fem_chpwr_adjustment
        phy.profile_outputs.MODEM_COH0_COHCHPWRTH2.override = 100 + fem_chpwr_adjustment

        fem_bbss_adjustment = 1

        phy.profile_outputs.MODEM_LONGRANGE4_LRCHPWRSH1.override = 3 + fem_bbss_adjustment
        phy.profile_outputs.MODEM_LONGRANGE4_LRCHPWRSH2.override = 4 + fem_bbss_adjustment
        phy.profile_outputs.MODEM_LONGRANGE4_LRCHPWRSH3.override = 5 + fem_bbss_adjustment
        phy.profile_outputs.MODEM_LONGRANGE4_LRCHPWRSH4.override = 5 + fem_bbss_adjustment
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH5.override = 6 + fem_bbss_adjustment
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH6.override = 7 + fem_bbss_adjustment
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH7.override = 8 + fem_bbss_adjustment
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH8.override = 9 + fem_bbss_adjustment
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH9.override = 10 + fem_bbss_adjustment
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH10.override = 11 + fem_bbss_adjustment
        phy.profile_outputs.MODEM_LONGRANGE5_LRCHPWRSH11.override = 12 + fem_bbss_adjustment

        return phy

    def PHY_IEEE802154_2p4GHz_antdiv_fastswitch(self, model, phy_name=None):
        phy = self.PHY_IEEE802154_2p4GHz_diversity(model, phy_name='PHY_IEEE802154_2p4GHz_antdiv_fastswitch')
        model.vars.zigbee_feature.value_forced = model.vars.zigbee_feature.var_enum.FCS


        phy.profile_inputs.antdiv_enable_parallel_correlation.value = False
        phy.profile_inputs.antdivmode.value = model.vars.antdivmode.var_enum.ANTSELFIRST
        phy.profile_inputs.antdivrepeatdis.value = model.vars.antdivrepeatdis.var_enum.NOREPEATFIRST

        phy.profile_inputs.synth_settling_mode.value = model.vars.synth_settling_mode.var_enum.FAST

        phy.profile_outputs.MODEM_ANTDIVCTRL_ENADPRETHRESH.override = 0x1
        phy.profile_outputs.MODEM_ANTDIVCTRL_ADPRETHRESH.override = 0x12
        phy.profile_outputs.MODEM_ADCTRL1_ADCTRL1.override = 1074331649
        phy.profile_outputs.MODEM_ADCTRL2_ADCTRL2.override = 1050625
        phy.profile_outputs.MODEM_ADPC1_ADPCWNDCNT.override = 2
        phy.profile_outputs.MODEM_ADPC1_ADPCTIMINGBAUDS.override = 0x40
        phy.profile_outputs.MODEM_ADPC2_ADPCCORRSAMPLES.override = 0x140
        phy.profile_outputs.MODEM_ADPC8_ADPCANTSAMPSWITCH.override = 0xb4
        phy.profile_outputs.MODEM_ADPC8_ADPCANTSAMPWRITE.override = 0x3c
        phy.profile_outputs.MODEM_ADPC8_ADPCANTSAMPBUF.override = 0x28

        phy.profile_outputs.MODEM_INTAFC_FOEPREAVG2.override = 4
        phy.profile_outputs.MODEM_INTAFC_FOEPREAVG1.override = 2
        phy.profile_outputs.MODEM_INTAFC_FOEPREAVG0.override = 1

        phy.profile_outputs.MODEM_CTRL6_TDREW.override = 0x20

        phy.profile_outputs.MODEM_TIMING_TIMINGBASES.override = 2

        phy.profile_outputs.MODEM_CTRL6_PSTIMABORT0.override = 1
        phy.profile_outputs.MODEM_CTRL6_PSTIMABORT1.override = 1
        phy.profile_outputs.MODEM_CTRL6_PSTIMABORT2.override = 1
        phy.profile_outputs.MODEM_CTRL5_LINCORR.override = 0

        phy.profile_outputs.MODEM_DCCOMP_DCCOMPGEAR.override = 7
        phy.profile_outputs.MODEM_RXRESTART_ANTSWRSTFLTTDIS.override = 0
        phy.profile_outputs.AGC_AGCPERIOD0_PERIODHI.override = 7
        phy.profile_outputs.AGC_AGCPERIOD1_PERIODLOW.override = 21
        phy.profile_outputs.AGC_HICNTREGION0_HICNTREGION0.override = 4
        phy.profile_outputs.AGC_HICNTREGION0_HICNTREGION1.override = 6
        phy.profile_outputs.AGC_HICNTREGION0_HICNTREGION2.override = 6
        phy.profile_outputs.AGC_HICNTREGION0_HICNTREGION3.override = 7
        phy.profile_outputs.AGC_HICNTREGION1_HICNTREGION4.override = 7

        phy.profile_outputs.RAC_SYTRIM1_SYTRIMMMDREGAMPBW.override = 0
        phy.profile_outputs.RAC_SYTRIM1_SYTRIMMMDREGAMPBIAS.override = 3

        return phy

    def PHY_Internal_IEEE802154_915MHz_cohdsa(self, model, phy_name=None):
        pass

    def PHY_Internal_IEEE802154_915MHz_cohdsa_antdiv(self, model, phy_name=None):
        pass

    def internal_ieee_802154_agc_overrides(self, phy, model):
        if phy.name in ['PHY_IEEE802154_2p4GHz_cohdsa',
                        'PHY_IEEE802154_2p4GHz_diversity',
                        'PHY_IEEE802154_2p4GHz_prod',
                        'PHY_IEEE802154_2p4GHz_diversity_prod',]:
            phy.profile_outputs.RAC_LNAMIXTRIM4_LNAMIXRFPKDTHRESHSELHI.override = 4
            phy.profile_outputs.AGC_GAINRANGE_PNGAINSTEP.override = 2
            phy.profile_outputs.AGC_CTRL4_PERIODRFPKD.override = 2000
            phy.profile_outputs.AGC_GAINRANGE_GAININCSTEP.override = 2

    def PHY_Internal_IEEE802154_868MHz_coh_thread_sf8(self, model, phy_name=None):
        pass

    def PHY_Internal_IEEE802154_915MHz_coh_thread_sf8(self, model, phy_name=None):
        pass
    