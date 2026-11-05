from pyradioconfig.parts.nixi.phys.Phys_connect import PHYS_connect_Nixi
from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pycalcmodel.core.common import IPCategory

class PHYS_connect_Lynx(IPhy):

    ip_category = IPCategory.LPW


    def PHY_Connect_2_4GHz_OQPSK_2Mcps_250kbps(self, model):
        phy = PHYS_connect_Nixi().PHY_Connect_2_4GHz_OQPSK_2Mcps_250kbps(model)

        return phy

