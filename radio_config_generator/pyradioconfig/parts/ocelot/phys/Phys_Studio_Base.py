from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pyradioconfig.parts.common.phys.phy_common import PHY_COMMON_FRAME_INTERNAL
from pycalcmodel.core.common import IPCategory



class PHYS_Studio_Base_Ocelot(IPhy):

    ip_category = IPCategory.LPW

    def _part_specific_phy_overrides(self, phy, model):
        pass

    default_xtal_frequency_hz = 39000000


    def Studio_2GFSK_base(self, phy, model):

        phy.profile_inputs.baudrate_tol_ppm.value = 0
        phy.profile_inputs.channel_spacing_hz.value = 1000000
        phy.profile_inputs.diff_encoding_mode.value = model.vars.diff_encoding_mode.var_enum.DISABLED
        phy.profile_inputs.dsss_chipping_code.value = 0
        phy.profile_inputs.dsss_len.value = 0
        phy.profile_inputs.dsss_spreading_factor.value = 0
        phy.profile_inputs.fsk_symbol_map.value = model.vars.fsk_symbol_map.var_enum.MAP0
        phy.profile_inputs.modulation_type.value = model.vars.modulation_type.var_enum.FSK2
        phy.profile_inputs.preamble_pattern.value = 1
        phy.profile_inputs.preamble_pattern_len.value = 2
        phy.profile_inputs.preamble_length.value = 40
        phy.profile_inputs.rx_xtal_error_ppm.value = 10
        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.Gaussian
        phy.profile_inputs.shaping_filter_param.value = 0.5
        phy.profile_inputs.syncword_0.value = 0xf68d
        phy.profile_inputs.syncword_1.value = 0x0
        phy.profile_inputs.syncword_length.value = 16
        phy.profile_inputs.tx_xtal_error_ppm.value = 10
        self._set_xtal_frequency(phy)

        PHY_COMMON_FRAME_INTERNAL(phy, model)

        self._part_specific_phy_overrides(phy, model)


    def PHY_Studio_915M_2GFSK_2Mbps_500K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='915M 2GFSK 2Mbps 500K', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 2000000
        phy.profile_inputs.deviation.value = 500000

        phy.profile_inputs.base_frequency_hz.value = 915000000

        return phy

    def PHY_Studio_915M_2GFSK_500Kbps_175K_mi0p7(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='915M 2GFSK 500Kbps 175K', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 500000
        phy.profile_inputs.deviation.value = 175000

        phy.profile_inputs.base_frequency_hz.value = 915000000

        return phy

    def PHY_Studio_915M_2GFSK_100Kbps_50K_antdiv(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='915M 2GFSK 100Kbps 50K antenna diversity',
                            phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 100000
        phy.profile_inputs.deviation.value = 50000

        phy.profile_inputs.base_frequency_hz.value = 915000000

        phy.profile_inputs.preamble_length.value = 60

        phy.profile_inputs.antdivmode.value = model.vars.antdivmode.var_enum.PHDEMODANTDIV
        phy.profile_inputs.skip2ant.value = model.vars.skip2ant.var_enum.SKIP2ANT

        return phy

    def PHY_Studio_915M_2GFSK_50Kbps_25K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='915M 2GFSK 50Kbps 25K', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 50000
        phy.profile_inputs.deviation.value = 25000

        phy.profile_inputs.base_frequency_hz.value = 915000000

        return phy

    def PHY_Studio_868M_2GFSK_50Kbps_25K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='868M 2GFSK 50Kbps 25K', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 50000
        phy.profile_inputs.deviation.value = 25000

        phy.profile_inputs.base_frequency_hz.value = 868000000

        return phy

    def PHY_Studio_868M_2GFSK_38p4Kbps_20K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='868M 2GFSK 38.4Kbps 20K', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 38400
        phy.profile_inputs.deviation.value = 20000

        phy.profile_inputs.base_frequency_hz.value = 868000000

        return phy

    def PHY_Datasheet_868M_2GFSK_2p4Kbps_1p2K_ETSI(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='868MHz 2GFSK 2.4Kbps 1.2KHz ETSI',
                            phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 2400
        phy.profile_inputs.deviation.value = 1200
        phy.profile_inputs.channel_spacing_hz.value = 25000

        phy.profile_inputs.base_frequency_hz.value = 868300000

        phy.profile_inputs.etsi_cat1_compatible.value = model.vars.etsi_cat1_compatible.var_enum.Band_868

        phy.profile_inputs.bandwidth_hz.value = 10000

        phy.profile_inputs.rx_xtal_error_ppm.value = 2
        phy.profile_inputs.tx_xtal_error_ppm.value = 2

        return phy

    def PHY_Studio_868M_2GFSK_600bps_800(self, model, phy_name=None, readable_name=None):
        if readable_name is None:
            readable_name = '868MHz 2GFSK 600bps 800Hz'
        phy = self._makePhy(model, model.profiles.Base, readable_name=readable_name, phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 600
        phy.profile_inputs.deviation.value = 800

        phy.profile_inputs.base_frequency_hz.value = 868000000

        phy.profile_inputs.rx_xtal_error_ppm.value = 5
        phy.profile_inputs.tx_xtal_error_ppm.value = 5

        return phy

    def PHY_Studio_490M_2GFSK_38p4Kbps_20K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='490MHz 2GFSK 38.4Kbps 20KHz', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 38400
        phy.profile_inputs.deviation.value = 20000

        phy.profile_inputs.base_frequency_hz.value = 490000000

        return phy

    def PHY_Datasheet_490M_2GFSK_10Kbps_25K_20ppm(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='490MHz 2GFSK 10Kbps 25KHz 20ppm', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 10000
        phy.profile_inputs.deviation.value = 25000

        phy.profile_inputs.base_frequency_hz.value = 490000000

        phy.profile_inputs.rx_xtal_error_ppm.value = 20
        phy.profile_inputs.tx_xtal_error_ppm.value = 20

        return phy

    def PHY_Studio_490M_2GFSK_10Kbps_5K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='490MHz 2GFSK 10Kbps 5KHz', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 10000
        phy.profile_inputs.deviation.value = 5000

        phy.profile_inputs.base_frequency_hz.value = 490000000

        return phy

    def PHY_Studio_490M_2GFSK_2p4Kbps_1p2K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='490MHz 2GFSK 2.4Kbps 1.2KHz', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 2400
        phy.profile_inputs.deviation.value = 1200

        phy.profile_inputs.base_frequency_hz.value = 490000000

        return phy

    def PHY_Studio_434M_2GFSK_100Kbps_50K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='434M 2GFSK 100Kbps 50K', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 100000
        phy.profile_inputs.deviation.value = 50000

        phy.profile_inputs.base_frequency_hz.value = 434000000

        return phy

    def PHY_Studio_434M_2GFSK_50Kbps_25K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='434M 2GFSK 50Kbps 25K', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 50000
        phy.profile_inputs.deviation.value = 25000

        phy.profile_inputs.base_frequency_hz.value = 434000000

        return phy

    def PHY_Studio_434M_2GFSK_2p4Kbps_1p2K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='434M 2GFSK 2.4Kbps 1.2K', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 2400
        phy.profile_inputs.deviation.value = 1200

        phy.profile_inputs.base_frequency_hz.value = 434000000

        return phy

    def PHY_Studio_315M_2GFSK_38p4Kbps_20K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='315MHz 2GFSK 38.4Kbps 20KHz', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 38400
        phy.profile_inputs.deviation.value = 20000

        phy.profile_inputs.base_frequency_hz.value = 315000000

        return phy

    def PHY_Studio_169M_2GFSK_38p4Kbps_20K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='169MHz 2GFSK 38.4Kbps 20KHz', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 38400
        phy.profile_inputs.deviation.value = 20000

        phy.profile_inputs.base_frequency_hz.value = 169000000

        return phy

    def PHY_Datasheet_169M_2GFSK_2p4Kbps_1p2K_ETSI(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='169MHz 2GFSK 2.4Kbps 1.2KHz ETSI',
                            phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 2400
        phy.profile_inputs.deviation.value = 1200
        phy.profile_inputs.channel_spacing_hz.value = 25000

        phy.profile_inputs.base_frequency_hz.value = 169000000

        phy.profile_inputs.etsi_cat1_compatible.value = model.vars.etsi_cat1_compatible.var_enum.Band_169

        phy.profile_inputs.bandwidth_hz.value = 10000

        phy.profile_inputs.rx_xtal_error_ppm.value = 8
        phy.profile_inputs.tx_xtal_error_ppm.value = 8

        return phy

    def PHY_Studio_169M_2GFSK_2p4Kbps_1p2K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='169MHz 2GFSK 2.4Kbps 1.2KHz', phy_name=phy_name)

        self.Studio_2GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 2400
        phy.profile_inputs.deviation.value = 1200

        phy.profile_inputs.base_frequency_hz.value = 169000000

        return phy



    def Studio_4GFSK_base(self, phy, model):

        phy.profile_inputs.baudrate_tol_ppm.value = 0
        phy.profile_inputs.channel_spacing_hz.value = 1000000
        phy.profile_inputs.diff_encoding_mode.value = model.vars.diff_encoding_mode.var_enum.DISABLED
        phy.profile_inputs.dsss_chipping_code.value = 0
        phy.profile_inputs.dsss_len.value = 0
        phy.profile_inputs.dsss_spreading_factor.value = 0
        phy.profile_inputs.fsk_symbol_map.value = model.vars.fsk_symbol_map.var_enum.MAP0
        phy.profile_inputs.modulation_type.value = model.vars.modulation_type.var_enum.FSK4
        phy.profile_inputs.preamble_pattern.value = 1
        phy.profile_inputs.preamble_pattern_len.value = 2
        phy.profile_inputs.preamble_length.value = 40
        phy.profile_inputs.rx_xtal_error_ppm.value = 10
        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.Gaussian
        phy.profile_inputs.shaping_filter_param.value = 1.0
        phy.profile_inputs.syncword_0.value = 0xf68d
        phy.profile_inputs.syncword_1.value = 0x0
        phy.profile_inputs.syncword_length.value = 16
        phy.profile_inputs.tx_xtal_error_ppm.value = 10
        self._set_xtal_frequency(phy)

        PHY_COMMON_FRAME_INTERNAL(phy, model)

        self._part_specific_phy_overrides(phy, model)


    def PHY_Studio_915M_4GFSK_200Kbps_16p6K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='915M 4GFSK 200Kbps 16.6K', phy_name=phy_name)

        self.Studio_4GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 200000
        phy.profile_inputs.deviation.value = 16666 #Inner symbol deviation

        phy.profile_inputs.base_frequency_hz.value = 915000000

        return phy

    def PHY_Studio_434M_4GFSK_50Kbps_8p33K(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='434M 4GFSK 50Kbps 8.33K', phy_name=phy_name)

        self.Studio_4GFSK_base(phy, model)

        phy.profile_inputs.bitrate.value = 50000
        phy.profile_inputs.deviation.value = 8330 #Inner symbol deviation

        phy.profile_inputs.base_frequency_hz.value = 434000000

        return phy



    def Studio_OOK_base(self, phy, model):
        phy.profile_inputs.baudrate_tol_ppm.value = 1000
        phy.profile_inputs.channel_spacing_hz.value = 1000000
        phy.profile_inputs.deviation.value = 0
        phy.profile_inputs.diff_encoding_mode.value = model.vars.diff_encoding_mode.var_enum.DISABLED
        phy.profile_inputs.dsss_chipping_code.value = 0
        phy.profile_inputs.dsss_len.value = 0
        phy.profile_inputs.dsss_spreading_factor.value = 0
        phy.profile_inputs.fsk_symbol_map.value = model.vars.fsk_symbol_map.var_enum.MAP0
        phy.profile_inputs.modulation_type.value = model.vars.modulation_type.var_enum.OOK
        phy.profile_inputs.preamble_pattern.value = 1
        phy.profile_inputs.preamble_pattern_len.value = 2
        phy.profile_inputs.preamble_length.value = 40
        phy.profile_inputs.rx_xtal_error_ppm.value = 10
        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.NONE
        phy.profile_inputs.shaping_filter_param.value = 1.5
        phy.profile_inputs.symbol_encoding.value = model.vars.symbol_encoding.var_enum.Manchester
        phy.profile_inputs.syncword_0.value = 0xf68d
        phy.profile_inputs.syncword_1.value = 0x0
        phy.profile_inputs.syncword_length.value = 16
        phy.profile_inputs.tx_xtal_error_ppm.value = 10
        self._set_xtal_frequency(phy)

        PHY_COMMON_FRAME_INTERNAL(phy, model)

        self._part_specific_phy_overrides(phy, model)


    def PHY_Studio_915M_OOK_120kbps(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='915M OOK 120kbps', phy_name=phy_name)

        self.Studio_OOK_base(phy, model)

        phy.profile_inputs.bitrate.value = 120000
        phy.profile_inputs.bandwidth_hz.value = 350000

        phy.profile_inputs.base_frequency_hz.value = 915000000

        phy.profile_inputs.symbol_encoding.value = model.vars.symbol_encoding.var_enum.NRZ

        return phy

    def PHY_Studio_915M_OOK_4p8kbps(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='915M OOK 4.8kbps Manchester', phy_name=phy_name)

        self.Studio_OOK_base(phy, model)

        phy.profile_inputs.bitrate.value = 4800
        phy.profile_inputs.bandwidth_hz.value = 350000

        phy.profile_inputs.base_frequency_hz.value = 915000000

        return phy

    def PHY_Studio_433M_OOK_4p8kbps(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='433M OOK 4.8kbps Manchester', phy_name=phy_name)

        self.Studio_OOK_base(phy, model)

        phy.profile_inputs.bitrate.value = 4800
        phy.profile_inputs.bandwidth_hz.value = 350000

        phy.profile_inputs.base_frequency_hz.value = 433920000

        return phy

    def PHY_Studio_315M_OOK_40kbps(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='315M OOK 40kbps Manchester', phy_name=phy_name)

        self.Studio_OOK_base(phy, model)

        phy.profile_inputs.bitrate.value = 40000
        phy.profile_inputs.bandwidth_hz.value = 350000

        phy.profile_inputs.base_frequency_hz.value = 315000000

        return phy

    def PHY_Studio_315M_OOK_4p8kbps(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='315M OOK 4.8kbps Manchester', phy_name=phy_name)

        self.Studio_OOK_base(phy, model)

        phy.profile_inputs.bitrate.value = 4800
        phy.profile_inputs.bandwidth_hz.value = 350000

        phy.profile_inputs.base_frequency_hz.value = 315000000

        return phy



    def Studio_GMSK_base(self, phy, model):
        """ Modulation Type """
        phy.profile_inputs.modulation_type.value = model.vars.modulation_type.var_enum.MSK

        """ Symbol Mapping and Encoding """
        phy.profile_inputs.fsk_symbol_map.value = model.vars.fsk_symbol_map.var_enum.MAP0
        phy.profile_inputs.diff_encoding_mode.value = model.vars.diff_encoding_mode.var_enum.DISABLED

        """ Baudrate """
        phy.profile_inputs.baudrate_tol_ppm.value = 0

        """ DSSS Parameters """
        phy.profile_inputs.dsss_chipping_code.value = 0
        phy.profile_inputs.dsss_len.value = 0
        phy.profile_inputs.dsss_spreading_factor.value = 0

        """ Shaping Filter Parameters """
        phy.profile_inputs.shaping_filter.value = model.vars.shaping_filter.var_enum.Gaussian
        phy.profile_inputs.shaping_filter_param.value = 0.5

        """ Preamble Parameters """
        phy.profile_inputs.preamble_pattern.value = 1
        phy.profile_inputs.preamble_pattern_len.value = 2
        phy.profile_inputs.preamble_length.value = 40

        """ Syncword Parameters """
        phy.profile_inputs.syncword_0.value = 0xf68d
        phy.profile_inputs.syncword_1.value = 0x0
        phy.profile_inputs.syncword_length.value = 16

        """ XO Parameters """
        self._set_xtal_frequency(phy)
        phy.profile_inputs.rx_xtal_error_ppm.value = 10
        phy.profile_inputs.tx_xtal_error_ppm.value = 10

        PHY_COMMON_FRAME_INTERNAL(phy, model)

        self._part_specific_phy_overrides(phy, model)

    def PHY_Studio_868M_GMSK_500Kbps(self, model, phy_name=None):
        phy = self._makePhy(model, model.profiles.Base, readable_name='868M GMSK 500Kbps', phy_name=phy_name)

        self.Studio_GMSK_base(phy, model)

        """ Frequency Planning """
        phy.profile_inputs.base_frequency_hz.value = 868000000
        phy.profile_inputs.channel_spacing_hz.value = 1000000

        """ Datarate / Bandwidth """
        phy.profile_inputs.bitrate.value = 500000
        phy.profile_inputs.deviation.value = 125000
        return phy
    pass # : End PHY_Studio_868M_GMSK_500Kbps
