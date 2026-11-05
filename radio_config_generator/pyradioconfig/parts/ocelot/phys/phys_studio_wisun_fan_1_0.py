from pyradioconfig.calculator_model_framework.decorators.phy_decorators import do_not_inherit_phys
from pyradioconfig.parts.jumbo.phys.phys_studio_wisun_fan_1_0 import PhysStudioWisunFanJumbo



class PhysStudioWisunFanOcelot(PhysStudioWisunFanJumbo):

    default_xtal_frequency_hz = 39000000

            