import numpy as np

from pyradioconfig.parts.lion.calculators.calc_shaping import CalcShapingLion


class CalcShapingCurl(CalcShapingLion):

    def calc_shaping_filter_gain_actual(self, model):

        shaping_filter_mode = model.vars.MODEM_CTRL0_SHAPING.value
        sf = self.get_shaping_filter(model)

        if shaping_filter_mode == 0:
            shaping_filter_gain = 127

        elif shaping_filter_mode == 1:
            shaping_filter_gain = np.average(
                [
                    sf[0] + sf[8] + sf[0],
                    sf[1] + sf[7],
                    sf[2] + sf[6],
                    sf[3] + sf[5],
                    sf[4] + sf[4],
                ]
            )

        elif shaping_filter_mode == 2:
            shaping_filter_gain = np.average(
                [
                    sf[0] + sf[7],
                    sf[1] + sf[6],
                    sf[2] + sf[5],
                    sf[3] + sf[4],
                ]
            )

        else:
            shaping_filter_gain = np.average(
                [
                    sf[0] + sf[8] + sf[16] + sf[24] + sf[32] + sf[40],
                    sf[1] + sf[9] + sf[17] + sf[25] + sf[33] + sf[41],
                    sf[2] + sf[10] + sf[18] + sf[26] + sf[34] + sf[42],
                    sf[3] + sf[11] + sf[19] + sf[27] + sf[35] + sf[43],
                    sf[4] + sf[12] + sf[20] + sf[28] + sf[36] + sf[44],
                    sf[5] + sf[13] + sf[21] + sf[29] + sf[37] + sf[45],
                    sf[6] + sf[14] + sf[22] + sf[30] + sf[38] + sf[46],
                    sf[7] + sf[15] + sf[23] + sf[31] + sf[39] + sf[47],
                ]
            )

        model.vars.shaping_filter_gain_actual.value = int(shaping_filter_gain)

    def get_shaping_filter(self, model):

        shaping_filter_mode = model.vars.MODEM_CTRL0_SHAPING.value
        c0 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING0_COEFF0.value,
                                                    model.vars.MODEM_SHAPING0_COEFF1.get_bit_width())
        c1 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING0_COEFF1.value,
                                                    model.vars.MODEM_SHAPING0_COEFF1.get_bit_width())
        c2 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING0_COEFF2.value,
                                                    model.vars.MODEM_SHAPING0_COEFF2.get_bit_width())
        c3 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING0_COEFF3.value,
                                                    model.vars.MODEM_SHAPING0_COEFF3.get_bit_width())
        c4 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING1_COEFF4.value,
                                                    model.vars.MODEM_SHAPING1_COEFF4.get_bit_width())
        c5 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING1_COEFF5.value,
                                                    model.vars.MODEM_SHAPING1_COEFF5.get_bit_width())
        c6 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING1_COEFF6.value,
                                                    model.vars.MODEM_SHAPING1_COEFF6.get_bit_width())
        c7 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING1_COEFF7.value,
                                                    model.vars.MODEM_SHAPING1_COEFF7.get_bit_width())
        c8 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING2_COEFF8.value,
                                                    model.vars.MODEM_SHAPING2_COEFF8.get_bit_width())
        c9 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING2_COEFF9.value,
                                                    model.vars.MODEM_SHAPING2_COEFF9.get_bit_width())
        c10 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING2_COEFF10.value,
                                                     model.vars.MODEM_SHAPING2_COEFF10.get_bit_width())
        c11 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING2_COEFF11.value,
                                                     model.vars.MODEM_SHAPING2_COEFF11.get_bit_width())
        c12 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING3_COEFF12.value,
                                                     model.vars.MODEM_SHAPING3_COEFF12.get_bit_width())
        c13 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING3_COEFF13.value,
                                                     model.vars.MODEM_SHAPING3_COEFF13.get_bit_width())
        c14 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING3_COEFF14.value,
                                                     model.vars.MODEM_SHAPING3_COEFF14.get_bit_width())
        c15 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING3_COEFF15.value,
                                                     model.vars.MODEM_SHAPING3_COEFF15.get_bit_width())
        c16 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING4_COEFF16.value,
                                                     model.vars.MODEM_SHAPING4_COEFF16.get_bit_width())
        c17 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING4_COEFF17.value,
                                                     model.vars.MODEM_SHAPING4_COEFF17.get_bit_width())
        c18 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING4_COEFF18.value,
                                                     model.vars.MODEM_SHAPING4_COEFF18.get_bit_width())
        c19 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING4_COEFF19.value,
                                                     model.vars.MODEM_SHAPING4_COEFF19.get_bit_width())
        c20 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING5_COEFF20.value,
                                                     model.vars.MODEM_SHAPING5_COEFF20.get_bit_width())
        c21 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING5_COEFF21.value,
                                                     model.vars.MODEM_SHAPING5_COEFF21.get_bit_width())
        c22 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING5_COEFF22.value,
                                                     model.vars.MODEM_SHAPING5_COEFF22.get_bit_width())
        c23 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING5_COEFF23.value,
                                                     model.vars.MODEM_SHAPING5_COEFF23.get_bit_width())
        c24 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING6_COEFF24.value,
                                                     model.vars.MODEM_SHAPING6_COEFF24.get_bit_width())
        c25 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING6_COEFF25.value,
                                                     model.vars.MODEM_SHAPING6_COEFF25.get_bit_width())
        c26 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING6_COEFF26.value,
                                                     model.vars.MODEM_SHAPING6_COEFF26.get_bit_width())
        c27 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING6_COEFF27.value,
                                                     model.vars.MODEM_SHAPING6_COEFF27.get_bit_width())
        c28 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING7_COEFF28.value,
                                                     model.vars.MODEM_SHAPING7_COEFF28.get_bit_width())
        c29 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING7_COEFF29.value,
                                                     model.vars.MODEM_SHAPING7_COEFF29.get_bit_width())
        c30 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING7_COEFF30.value,
                                                     model.vars.MODEM_SHAPING7_COEFF30.get_bit_width())
        c31 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING7_COEFF31.value,
                                                     model.vars.MODEM_SHAPING7_COEFF31.get_bit_width())
        c32 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING8_COEFF32.value,
                                                     model.vars.MODEM_SHAPING8_COEFF32.get_bit_width())
        c33 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING8_COEFF33.value,
                                                     model.vars.MODEM_SHAPING8_COEFF33.get_bit_width())
        c34 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING8_COEFF34.value,
                                                     model.vars.MODEM_SHAPING8_COEFF34.get_bit_width())
        c35 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING8_COEFF35.value,
                                                     model.vars.MODEM_SHAPING8_COEFF35.get_bit_width())
        c36 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING9_COEFF36.value,
                                                     model.vars.MODEM_SHAPING9_COEFF36.get_bit_width())
        c37 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING9_COEFF37.value,
                                                     model.vars.MODEM_SHAPING9_COEFF37.get_bit_width())
        c38 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING9_COEFF38.value,
                                                     model.vars.MODEM_SHAPING9_COEFF38.get_bit_width())
        c39 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING9_COEFF39.value,
                                                     model.vars.MODEM_SHAPING9_COEFF39.get_bit_width())
        c40 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING10_COEFF40.value,
                                                     model.vars.MODEM_SHAPING10_COEFF40.get_bit_width())
        c41 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING10_COEFF41.value,
                                                     model.vars.MODEM_SHAPING10_COEFF41.get_bit_width())
        c42 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING10_COEFF42.value,
                                                     model.vars.MODEM_SHAPING10_COEFF42.get_bit_width())
        c43 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING10_COEFF43.value,
                                                     model.vars.MODEM_SHAPING10_COEFF43.get_bit_width())
        c44 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING11_COEFF44.value,
                                                     model.vars.MODEM_SHAPING11_COEFF44.get_bit_width())
        c45 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING11_COEFF45.value,
                                                     model.vars.MODEM_SHAPING11_COEFF45.get_bit_width())
        c46 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING11_COEFF46.value,
                                                     model.vars.MODEM_SHAPING11_COEFF46.get_bit_width())
        c47 = self._covert_twos_complement_to_signed(model.vars.MODEM_SHAPING11_COEFF47.value,
                                                     model.vars.MODEM_SHAPING11_COEFF47.get_bit_width())

        if shaping_filter_mode == 0:
            shaping_filter = np.array([127, 127, 127, 127, 127, 127, 127, 127])

        elif shaping_filter_mode == 1:
            shaping_filter = np.array([c0, c1, c2, c3, c4, c5, c6, c7, c8, c7, c6, c5, c4, c3, c2, c1, c0])

        elif shaping_filter_mode == 2:
            shaping_filter = np.array([c0, c1, c2, c3, c4, c5, c6, c7, c7, c6, c5, c4, c3, c2, c1, c0])

        else:
            shaping_filter = np.array(
                [
                    c0,  c1,  c2,  c3,  c4,  c5,  c6,  c7,  c8,  c9, c10, c11, c12, c13, c14, c15,
                    c16, c17, c18, c19, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30, c31,
                    c32, c33, c34, c35, c36, c37, c38, c39, c40, c41, c42, c43, c44, c45, c46, c47
                ])
        return shaping_filter
