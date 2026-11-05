from pyradioconfig.parts.lion.phys.Phys_Studio_Connect import PhysStudioConnectLion


class PhysStudioConnectCurl(PhysStudioConnectLion):

    def PHY_Connect_2_4GHz_OQPSK_2Mcps_250kbps(self, model, phy_name=None):
        phy = super().PHY_Connect_2_4GHz_OQPSK_2Mcps_250kbps(model)

        self._set_xtal_frequency(phy, xtal_freq_hz=40000000)

        return phy
