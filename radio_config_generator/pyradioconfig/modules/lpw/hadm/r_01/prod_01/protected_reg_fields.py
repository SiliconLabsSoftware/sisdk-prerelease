from pyradioconfig.calculator_model_framework.interfaces.ipcalculator import IPRegFields


class HadmProtectedRegFields(IPRegFields):
    """
    These registers are not owned by the Radio Calculator post PG as per the RDM
    (Register Data Model), so they cannot be written to on Target IC PHYs.

    This class declares the protected register list, allowing design engineers to
    configure these registers via Radio Calculator for Target Sim and Target FPGA PHYs.

    To add a new protected register:
      1. Add the register field string to `reg_field_list` in this file.
      2. write into the protected registers using `_ip_protected_reg_write()`.
    """

    def __init__(self):
        super().__init__()
        self._is_protected_reg_field = True
        self.reg_field_list = []
