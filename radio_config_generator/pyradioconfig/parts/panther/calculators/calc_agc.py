import math

from pyradioconfig.calculator_model_framework.Utils.LogMgr import LogMgr
from pyradioconfig.parts.nixi.calculators.calc_agc import CALC_AGC_nixi
from pyradioconfig.calculator_model_framework.Utils.CustomExceptions import CalculationException
from py_2_and_3_compatibility import *
from pycalcmodel.core.variable import ModelVariableFormat


class CALC_AGC_panther(CALC_AGC_nixi):

    def buildVariables(self, model):

        super().buildVariables(model)

        self._addModelActual(model, 'cfloopdel_us', float, ModelVariableFormat.DECIMAL)

    def calc_agc_misc(self, model):
        """Sets the agc variables to some default state until we figure out how to set them

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        self._reg_write(model.vars.AGC_CTRL0_ADCRESETDURATION,    0)

        self._reg_write(model.vars.AGC_CTRL1_SUBPERIOD, 			0)
        self._reg_write(model.vars.AGC_CTRL1_SUBINT,			    0)
        self._reg_write(model.vars.AGC_CTRL1_SUBNUM,              0)
        self._reg_write(model.vars.AGC_CTRL1_SUBDEN, 				0)


        self._reg_write(model.vars.AGC_MANGAIN_MANGAININDEX, 		0)
        self._reg_write(model.vars.AGC_MANGAIN_MANGAININDEXEN,	0)


        etsi = model.vars.etsi_cat1_compatible.value
        if etsi == model.vars.etsi_cat1_compatible.var_enum.Band_169:
            self._reg_write(model.vars.AGC_IFPEAKDET_PKDTHRESH2,	12)
            self._reg_write(model.vars.AGC_IFPEAKDET_PKDTHRESH1,	6)
        else:
            self._reg_write(model.vars.AGC_IFPEAKDET_PKDTHRESH2,	8)
            self._reg_write(model.vars.AGC_IFPEAKDET_PKDTHRESH1,	2)

        self._reg_write(model.vars.AGC_AGCPERIOD_SETTLETIMEIF,6)
        self._reg_write(model.vars.AGC_AGCPERIOD_SETTLETIMERF,13)
        if model.part_family.lower() in ["panther"]:
            self._reg_write(model.vars.AGC_CTRL0_AGCCLKUNDIVREQ,0)
        self._reg_write(model.vars.AGC_CTRL0_AGCRST, 0)
        self._reg_write(model.vars.AGC_CTRL0_DSADISCFLOOP, 0)
        self._reg_write(model.vars.AGC_CTRL0_ENRSSIRESET, 0)
        self._reg_write(model.vars.AGC_CTRL1_SUBDEN, 0)
        self._reg_write(model.vars.AGC_CTRL1_SUBINT, 0)
        self._reg_write(model.vars.AGC_CTRL1_SUBNUM, 0)
        self._reg_write(model.vars.AGC_CTRL1_SUBPERIOD, 0)
        self._reg_write(model.vars.AGC_CTRL2_DMASEL, 0)
        self._reg_write(model.vars.AGC_CTRL2_PRSDEBUGEN, 0)
        self._reg_write(model.vars.AGC_GAINRANGE_HIPWRTHD, 8)
        self._reg_write(model.vars.AGC_GAINRANGE_LATCHEDHISTEP, 1)
        self._reg_write(model.vars.AGC_GAINRANGE_LNAINDEXBORDER, 7)
        self._reg_write(model.vars.AGC_GAINSTEPLIM_MAXPWRVAR, 0)
        self._reg_write(model.vars.AGC_GAINSTEPLIM_TRANRSTAGC, 0)
        self._reg_write(model.vars.AGC_LBT_CCARSSIPERIOD, 0)
        self._reg_write(model.vars.AGC_LBT_ENCCAGAINREDUCED, 0)
        self._reg_write(model.vars.AGC_LBT_ENCCARSSIMAX, 0)
        self._reg_write(model.vars.AGC_LBT_ENCCARSSIPERIOD, 0)
        self._reg_write(model.vars.AGC_LNAMIXCODE0_LNAMIXSLICE1, 61)
        self._reg_write(model.vars.AGC_LNAMIXCODE0_LNAMIXSLICE2, 46)
        self._reg_write(model.vars.AGC_LNAMIXCODE0_LNAMIXSLICE3, 36)
        self._reg_write(model.vars.AGC_LNAMIXCODE0_LNAMIXSLICE4, 28)
        self._reg_write(model.vars.AGC_LNAMIXCODE0_LNAMIXSLICE5, 21)
        self._reg_write(model.vars.AGC_LNAMIXCODE1_LNAMIXSLICE10, 5)
        self._reg_write(model.vars.AGC_LNAMIXCODE1_LNAMIXSLICE6, 17)
        self._reg_write(model.vars.AGC_LNAMIXCODE1_LNAMIXSLICE7, 12)
        self._reg_write(model.vars.AGC_LNAMIXCODE1_LNAMIXSLICE8, 10)
        self._reg_write(model.vars.AGC_LNAMIXCODE1_LNAMIXSLICE9, 6)
        self._reg_write(model.vars.AGC_MANGAIN_MANGAINEN, 0)
        self._reg_write(model.vars.AGC_MANGAIN_MANGAINIFPGA, 0)
        self._reg_write(model.vars.AGC_MANGAIN_MANGAINLNA, 0)
        self._reg_write(model.vars.AGC_MANGAIN_MANGAINPN, 0)
        self._reg_write(model.vars.AGC_MANGAIN_MANIFHILATRST, 0)
        self._reg_write(model.vars.AGC_MANGAIN_MANIFLOLATRST, 0)
        self._reg_write(model.vars.AGC_MANGAIN_MANRFLATRST, 0)
        self._reg_write(model.vars.AGC_PGACODE0_PGAGAIN1, 0)
        self._reg_write(model.vars.AGC_PGACODE0_PGAGAIN2, 1)
        self._reg_write(model.vars.AGC_PGACODE0_PGAGAIN3, 2)
        self._reg_write(model.vars.AGC_PGACODE0_PGAGAIN4, 3)
        self._reg_write(model.vars.AGC_PGACODE0_PGAGAIN5, 4)
        self._reg_write(model.vars.AGC_PGACODE0_PGAGAIN6, 5)
        self._reg_write(model.vars.AGC_PGACODE0_PGAGAIN7, 6)
        self._reg_write(model.vars.AGC_PGACODE0_PGAGAIN8, 7)
        self._reg_write(model.vars.AGC_PGACODE1_PGAGAIN10, 9)
        self._reg_write(model.vars.AGC_PGACODE1_PGAGAIN11, 10)
        self._reg_write(model.vars.AGC_PGACODE1_PGAGAIN9, 8)
        self._reg_write(model.vars.AGC_PNRFATT0_LNAMIXRFATT1, 0)
        self._reg_write(model.vars.AGC_PNRFATT0_LNAMIXRFATT2, 1)
        self._reg_write(model.vars.AGC_PNRFATT0_LNAMIXRFATT3, 2)
        self._reg_write(model.vars.AGC_PNRFATT0_LNAMIXRFATT4, 4)
        self._reg_write(model.vars.AGC_PNRFATT0_LNAMIXRFATT5, 6)
        self._reg_write(model.vars.AGC_PNRFATT1_LNAMIXRFATT10, 24)
        self._reg_write(model.vars.AGC_PNRFATT1_LNAMIXRFATT6, 8)
        self._reg_write(model.vars.AGC_PNRFATT1_LNAMIXRFATT7, 11)
        self._reg_write(model.vars.AGC_PNRFATT1_LNAMIXRFATT8, 15)
        self._reg_write(model.vars.AGC_PNRFATT1_LNAMIXRFATT9, 18)
        self._reg_write(model.vars.AGC_PNRFATT2_LNAMIXRFATT11, 31)
        self._reg_write(model.vars.AGC_PNRFATT2_LNAMIXRFATT12, 32)
        self._reg_write(model.vars.AGC_PNRFATT2_LNAMIXRFATT13, 46)
        self._reg_write(model.vars.AGC_PNRFATT2_LNAMIXRFATT14, 61)
        self._reg_write(model.vars.AGC_RSSISTEPTHR_RSSIFAST, 0)


        if model.part_family.lower() in ["panther"]:
            self._reg_write(model.vars.AGC_STATUS0_ADCINDEX, 0)
        self._reg_write(model.vars.AGC_GAINRANGE_PGAINDEXBORDER, 8)

    def calc_agc_reg(self, model):
        """given frequency band select between two AGC settings

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        subgig_band = model.vars.subgig_band.value

        self._reg_write(model.vars.AGC_GAININDEX_MININDEXDEGEN, 	    0)
        self._reg_write(model.vars.AGC_GAININDEX_MININDEXPGA, 		0)
        self._reg_write(model.vars.AGC_GAININDEX_NUMINDEXSLICES, 	    6)
        self._reg_write(model.vars.AGC_GAININDEX_NUMINDEXDEGEN, 	    3)
        self._reg_write(model.vars.AGC_GAININDEX_NUMINDEXPGA, 		12)
        self._reg_write(model.vars.AGC_MANGAIN_MANGAINLNASLICES,  0)
        self._reg_write(model.vars.AGC_MANGAIN_MANGAINLNASLICESREG, 0)

        if subgig_band:
            self._reg_write(model.vars.AGC_GAINRANGE_MAXGAIN, 			62)
            self._reg_write(model.vars.AGC_GAINRANGE_MINGAIN, 			112)
            self._reg_write(model.vars.AGC_GAININDEX_NUMINDEXATTEN, 	    18)
            self._reg_write(model.vars.AGC_MANGAIN_MANGAINLNAATTEN,  0)

        else:
            self._reg_write(model.vars.AGC_GAINRANGE_MAXGAIN, 			60)
            self._reg_write(model.vars.AGC_GAINRANGE_MINGAIN, 			122)
            self._reg_write(model.vars.AGC_GAININDEX_NUMINDEXATTEN, 	    12)
            self._reg_write(model.vars.AGC_MANGAIN_MANGAINLNAATTEN,  12)

    def calc_hyst_reg(self, model):
        """program HYST register

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        hyst = model.vars.agc_hysteresis.value

        if hyst > 15:
            hyst = 15

        self._reg_write(model.vars.AGC_GAINSTEPLIM_HYST, hyst)


    def calc_fastloopdel_reg(self, model):
        """calculate FASTLOOPDEL based on corresponding delay

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        fxo = model.vars.xtal_frequency.value
        fast_loop_delay = model.vars.fast_loop_delay.value

        delay = int(math.ceil(fast_loop_delay * fxo))

        if delay < 0:
            delay = 0
        elif delay > 31:
            delay = 31


    def calc_cfloopdel_reg(self, model):
        """program CFLOOPDEL based on calculated value

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        cfloopdel = model.vars.agc_settling_delay.value

        self._reg_write(model.vars.AGC_GAINSTEPLIM_CFLOOPDEL, cfloopdel)

    def calc_agc_faststepup_reg(self, model):
        """set FASTSTEPUP based on modulation method

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        mod_format = model.vars.modulation_type.value
        etsi = model.vars.etsi_cat1_compatible.value

        if etsi == model.vars.etsi_cat1_compatible.var_enum.Band_169:
            step = 1
        else:
            step = 2


    def calc_faststepdown_reg(self, model):
        """choose FASTSTEPDOWN value based on agc_speed

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        agc_speed = model.vars.agc_speed.value
        etsi = model.vars.etsi_cat1_compatible.value

        if etsi == model.vars.etsi_cat1_compatible.var_enum.Band_169:
            step = 1
        else:
            if agc_speed == model.vars.agc_speed.var_enum.FAST.value:
                step = 5
            elif agc_speed == model.vars.agc_speed.var_enum.SLOW.value:
                step = 3  # TODO: come up with a value for this one
            else:
                step = 3


    def calc_agc_adcattenmode_code(self, model):

        etsi = model.vars.etsi_cat1_compatible.value

        if etsi != model.vars.etsi_cat1_compatible.var_enum.Normal:
            self._reg_write(model.vars.AGC_CTRL0_ADCATTENCODE, 1)
            self._reg_write(model.vars.AGC_CTRL0_ADCATTENMODE, 1)
        else:
            self._reg_write(model.vars.AGC_CTRL0_ADCATTENCODE, 0)
            self._reg_write(model.vars.AGC_CTRL0_ADCATTENMODE, 0)


    def calc_agc_cfloopstepmax_reg(self, model):
        """set CFLOOPSTEPMAX based on modulation method

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        mod_format = model.vars.modulation_type.value

        if mod_format == model.vars.modulation_type.var_enum.OOK:
            step = 5
        elif mod_format == model.vars.modulation_type.var_enum.FSK4:
            step = 1
        else:
            step = 8

        self._reg_write(model.vars.AGC_GAINSTEPLIM_CFLOOPSTEPMAX, step)

    def calc_fastloopdel_reg(self, model):
        """calculate FASTLOOPDEL based on corresponding delay

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        fxo = model.vars.xtal_frequency.value
        fast_loop_delay = model.vars.fast_loop_delay.value

        delay = int(math.ceil(fast_loop_delay * fxo))

        if delay < 0:
            delay = 0
        elif delay > 31:
            delay = 31


    def calc_lnaslicesdel_reg(self, model):
        """calculate LNASLICESDEL based on corresponding delay

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        fxo = model.vars.xtal_frequency.value
        lna_slices_delay = model.vars.lna_slices_delay.value

        delay = int(math.ceil(lna_slices_delay * fxo))

        if delay < 0:
            delay = 0
        elif delay > 31:
            delay = 31


    def calc_ifpgadel_reg(self, model):
        """calculate IFPGADEL based on corresponding delay

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        fxo = model.vars.xtal_frequency.value
        if_pga_delay = model.vars.if_pga_delay.value

        delay = int(math.ceil(if_pga_delay * fxo))

        if delay < 0:
            delay = 0
        elif delay > 31:
            delay = 31


    def calc_pkdwait_reg(self, model):
        """calculate PKDWAIT based on agc_speed

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        pass


    def calc_agc_series2(self, model):
        """Calculate AGC parameters specific to Series 2 (Panther, Lynx, Ocelot)
        There were formerly written to reset/defaults in calc_agc_misc above

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        mod_format = model.vars.modulation_type.value
        fxo = model.vars.fxo_or_fdec8.value
        f_if = float(model.vars.if_frequency_hz.value)

        if mod_format == model.vars.modulation_type.var_enum.OOK:
            LogMgr.Warning("  WARNING: Series 2 AGC does not calculate for OOK, using AGC defaults!")
            self._reg_write(model.vars.AGC_AGCPERIOD_PERIODHI, 14)
            self._reg_write(model.vars.AGC_AGCPERIOD_PERIODLO, 55)

            self._reg_write(model.vars.AGC_STEPDWN_STEPDWN0, 1)
            self._reg_write(model.vars.AGC_STEPDWN_STEPDWN1, 2)
            self._reg_write(model.vars.AGC_STEPDWN_STEPDWN2, 4)
            self._reg_write(model.vars.AGC_STEPDWN_STEPDWN3, 6)
            self._reg_write(model.vars.AGC_STEPDWN_STEPDWN4, 6)
            self._reg_write(model.vars.AGC_STEPDWN_STEPDWN5, 6)

            self._reg_write(model.vars.AGC_HICNTREGION_HICNTREGION0, 3)
            self._reg_write(model.vars.AGC_HICNTREGION_HICNTREGION1, 4)
            self._reg_write(model.vars.AGC_HICNTREGION_HICNTREGION2, 5)
            self._reg_write(model.vars.AGC_HICNTREGION_HICNTREGION3, 6)
            self._reg_write(model.vars.AGC_HICNTREGION_HICNTREGION4, 8)

            self._reg_write(model.vars.AGC_AGCPERIOD_MAXHICNTTHD, 7)

            self._reg_write(model.vars.AGC_CTRL2_SAFEMODE, 1)
            self._reg_write(model.vars.AGC_CTRL2_SAFEMODETHD, 2)

            self._reg_write(model.vars.AGC_CTRL2_REHICNTTHD, 8)
            self._reg_write(model.vars.AGC_CTRL2_RELBYCHPWR, 0)
            self._reg_write(model.vars.AGC_CTRL2_RELOTHD, 3)
            self._reg_write(model.vars.AGC_CTRL2_RELTARGETPWR, 0)
            self._reg_write(model.vars.AGC_CTRL2_SAFEMODE, 1)
            self._reg_write(model.vars.AGC_CTRL2_SAFEMODETHD, 2)
            self._reg_write(model.vars.AGC_CTRL3_IFPKDDEB, 0)
            self._reg_write(model.vars.AGC_CTRL3_IFPKDDEBPRD, 0)
            self._reg_write(model.vars.AGC_CTRL3_IFPKDDEBRST, 4)
            self._reg_write(model.vars.AGC_CTRL3_IFPKDDEBTHD, 0)
            self._reg_write(model.vars.AGC_CTRL3_RFPKDDEB, 1)
            self._reg_write(model.vars.AGC_CTRL3_RFPKDDEBPRD, 40)
            self._reg_write(model.vars.AGC_CTRL3_RFPKDDEBRST, 10)
            self._reg_write(model.vars.AGC_CTRL3_RFPKDDEBTHD,2)

            self._reg_write(model.vars.AGC_GAINRANGE_BOOSTLNA, 0)
            self._reg_write(model.vars.AGC_GAINRANGE_GAININCSTEP, 1)
            self._reg_write(model.vars.AGC_GAINRANGE_LNABWADJ, 0)
            self._reg_write(model.vars.AGC_GAINRANGE_PNGAINSTEP, 3)

            self._reg_write(model.vars.AGC_CTRL0_DISCFLOOPADJ, 0)
            self._reg_write(model.vars.AGC_CTRL0_DISPNDWNCOMP, 0)
            self._reg_write(model.vars.AGC_CTRL0_DISPNGAINUP, 1)
            self._reg_write(model.vars.AGC_CTRL0_DISRESETCHPWR, 0)

            self._reg_write(model.vars.AGC_CTRL1_PWRPERIOD, 1)

            self._reg_write(model.vars.AGC_CTRL2_DISRFPKD, 0)


            pass
        else:
            periodhi = int(round (fxo / ( 2 * f_if)))
            periodlo = 3 * periodhi

            if periodlo > 255:
                LogMgr.Warning("  WARNING:  AGC_AGCPERIOD_PERIODLO {} overflow. Saturating value to 255 !".format(periodlo))
                periodlo = 255

            if periodhi > 255:
                LogMgr.Warning("  WARNING:  AGC_AGCPERIOD_PERIODHI {} overflow. Saturating value to 255 !".format(periodhi))
                periodhi = 255

            self._reg_write(model.vars.AGC_AGCPERIOD_PERIODHI, periodhi)
            self._reg_write(model.vars.AGC_AGCPERIOD_PERIODLO, periodlo)

            self._reg_write(model.vars.AGC_STEPDWN_STEPDWN0, 1)
            self._reg_write(model.vars.AGC_STEPDWN_STEPDWN1, 2)
            self._reg_write(model.vars.AGC_STEPDWN_STEPDWN2, 4)
            self._reg_write(model.vars.AGC_STEPDWN_STEPDWN3, 3)
            self._reg_write(model.vars.AGC_STEPDWN_STEPDWN4, 4)
            self._reg_write(model.vars.AGC_STEPDWN_STEPDWN5, 5)

            hicntregion0 = int(py2round(0.55 * periodhi))
            hicntregion1 = int(py2round(0.75 * periodhi))
            hicntregion2 = int(py2round(0.85 * periodhi))
            hicntregion3 = int(py2round(0.90 * periodhi))
            hicntregion4 = int(py2round(0.93 * periodhi))

            if (hicntregion0 > 15) or (hicntregion1> 15):
                pass

            if (hicntregion0 > 15):
                LogMgr.Warning("  WARNING: AGC_HICNTREGION_HICNTREGION 0 calculated beyond range: hicntregion0 {}, hicntregion1 {}, AGC_AGCPERIOD_PERIODHI {}. Saturating value to 15 !".format(hicntregion0,hicntregion1,periodhi))
                hicntregion0 = 15

            if (hicntregion1 > 15):
                LogMgr.Warning("  WARNING: AGC_HICNTREGION_HICNTREGION 1 calculated beyond range: hicntregion0 {}, hicntregion1 {}, AGC_AGCPERIOD_PERIODHI {}. Saturating value to 15 !".format(hicntregion0,hicntregion1,periodhi))
                hicntregion1 = 15

            self._reg_write(model.vars.AGC_HICNTREGION_HICNTREGION0, hicntregion0)
            self._reg_write(model.vars.AGC_HICNTREGION_HICNTREGION1, hicntregion1)
            self._reg_write(model.vars.AGC_HICNTREGION_HICNTREGION2, hicntregion2)
            self._reg_write(model.vars.AGC_HICNTREGION_HICNTREGION3, hicntregion3)
            self._reg_write(model.vars.AGC_HICNTREGION_HICNTREGION4, hicntregion4)

            self._reg_write(model.vars.AGC_AGCPERIOD_MAXHICNTTHD, hicntregion4)

            self._reg_write(model.vars.AGC_CTRL2_SAFEMODE, 1)
            self._reg_write(model.vars.AGC_CTRL2_SAFEMODETHD, 2)

            rehicntthd = hicntregion0 - 1
            self._reg_write(model.vars.AGC_CTRL2_REHICNTTHD, rehicntthd)

            self._reg_write(model.vars.AGC_CTRL2_RELOTHD, 4)
            self._reg_write(model.vars.AGC_CTRL2_RELBYCHPWR, 3)
            self._reg_write(model.vars.AGC_CTRL2_RELTARGETPWR, 236)

            self._reg_write(model.vars.AGC_GAINRANGE_GAININCSTEP, 1)

            self._reg_write(model.vars.AGC_CTRL3_IFPKDDEB, 0) #Known issue with IFPKD latch, so this is forced to 1 in many PHYs
            self._reg_write(model.vars.AGC_CTRL3_IFPKDDEBPRD, 0) # don't care, reset/default value
            self._reg_write(model.vars.AGC_CTRL3_IFPKDDEBRST, 4) # don't care, reset/default value
            self._reg_write(model.vars.AGC_CTRL3_IFPKDDEBTHD, 0) # don't care, reset/default value


            self._reg_write(model.vars.AGC_CTRL1_PWRPERIOD, 1)

            self._reg_write(model.vars.AGC_GAINRANGE_BOOSTLNA, 0)
            self._reg_write(model.vars.AGC_GAINRANGE_LNABWADJ, 0)

            self._reg_write(model.vars.AGC_CTRL2_DISRFPKD, 0)
            self._reg_write(model.vars.AGC_CTRL0_DISPNDWNCOMP, 0) # don't care, reset/default value
            self._reg_write(model.vars.AGC_CTRL0_DISPNGAINUP, 1) # don't care, reset/default value
            self._reg_write(model.vars.AGC_GAINRANGE_PNGAINSTEP, 3) # don't care, reset/default value
            self._reg_write(model.vars.AGC_CTRL3_RFPKDDEB, 1)  # don't care, reset/default value
            self._reg_write(model.vars.AGC_CTRL3_RFPKDDEBPRD, 40)   # don't care, reset/default value
            self._reg_write(model.vars.AGC_CTRL3_RFPKDDEBRST, 10)  # don't care, reset/default value
            self._reg_write(model.vars.AGC_CTRL3_RFPKDDEBTHD, 2)  # don't care, reset/default value

            self._reg_write(model.vars.AGC_CTRL0_DISCFLOOPADJ, 1)
            self._reg_write(model.vars.AGC_CTRL0_DISRESETCHPWR, 0)

    def calc_agc_cfloopdel_actual(self, model):
        reg = model.vars.AGC_GAINSTEPLIM_CFLOOPDEL.value
        fxo = model.vars.fxo_or_fdec8.value
        dec0 = model.vars.dec0_actual.value
        dec1 = model.vars.dec1_actual.value
        src1_actual = model.vars.src1_ratio_actual.value
        src2_actual = model.vars.src2_ratio_actual.value

        fsrc2 = fxo / dec0 / dec1 * src1_actual * src2_actual
        model.vars.cfloopdel_us_actual.value = reg / fsrc2 * 1e6

        return

    def calc_slowdecaycnt_reg(self, model):
        pass