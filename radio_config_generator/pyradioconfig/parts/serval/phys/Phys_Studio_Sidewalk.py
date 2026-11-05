from pyradioconfig.calculator_model_framework.interfaces.iphy import IPhy
from pyradioconfig.parts.margay.phys.Phys_Studio_Sidewalk import PhysStudioSidewalkMargay
from pycalcmodel.core.common import IPCategory


class Phys_Studio_Sidewalk_Serval(IPhy):

    ip_category = IPCategory.LPW

    def PHY_Sidewalk_2GFSK_50Kbps(self, model):
        phy = PhysStudioSidewalkMargay().PHY_Sidewalk_2GFSK_50Kbps(model)
