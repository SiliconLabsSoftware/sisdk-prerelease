from pyradioconfig.calculator_model_framework.Utils.LogMgr import LogMgr
from pyradioconfig.calculator_model_framework.interfaces.icalculator import ICalculator
from pycalcmodel.core.variable import ModelVariableFormat


"""
Calculations interface file
"""

class IPRegFields(object):
    """
    Base class for all IP register fields.
    This class provides a common interface for all IP register fields to use.
    Class variables:
        reg_field_list : tuple list of all the registers for the peripheral
        _is_protected_reg_field : boolean to indicate if the listed reg fields are protected or not
    """
    def __init__(self):
        self.reg_field_list = []
        self._is_protected_reg_field = False
    pass

class IPCalculator(ICalculator):
    """
    Base class for all IP calculators.
    This class provides a common interface for all IP calculators to use.
    Class variables:
        _peripheral_name : name of the peripheral
        _reg_field_list : list of all the registers for the peripheral
        _protected_reg_field_list : ProtectedRegField list for the IP, these are to be added to the model variables only
            if model target is not in protected_reg_field_skip_targets.
        _protected_reg_field_skip_target : model.target for which the _protected_reg_field_list
            should not be added to the model variables. Use skip_protected_reg_fields_for_target() rather than reading
            this value directly, so that model.vars (buildVariables) and profile.outputs
            (IPCollector_Base.build_peripheral_regs) stay consistent.
    """
    _peripheral_name = ""
    _reg_field_list = []
    _protected_reg_field_list = []
    _protected_reg_field_skip_target = 'IC'

    def __init__(self, peripheral_name):
        self._peripheral_name = peripheral_name

    @classmethod
    def should_skip_protected_reg_fields_for_target(cls, target):
        """
        Single source of truth for whether protected reg fields should be skipped for a given
        model.target. Used both when adding protected reg fields to model.vars (buildVariables)
        and when adding them to profile.outputs (IPCollector_Base.build_peripheral_regs), so the
        two stay in sync.
        """
        return target.strip().upper() == cls._protected_reg_field_skip_target.upper()

    def buildVariables(self, modem_model):
        self._add_registers_to_model_variable(modem_model, self._reg_field_list)
        if not self.should_skip_protected_reg_fields_for_target(modem_model.target):
            self._add_registers_to_model_variable(modem_model, self._protected_reg_field_list)

    def _add_registers_to_model_variable(self, model, reg_field_list):
        for reg_field in reg_field_list:
            reg_field_name = self._peripheral_name + "." + reg_field
            self._addModelRegister(model, reg_field_name, int, ModelVariableFormat.HEX)

    def _ip_reg_write(self, model, reg_field_name, field_value=None,
                      default=False, do_not_care=False, limit_upper=None, limit_lower=None,
                      check_saturation=False, allow_neg=False, neg_twos_comp=True):
        """
        This is a thin wrapper around _reg_write that automatically prepends the peripheral name to the register field
        name, so that the caller can just pass in the register field name without the peripheral name.
        """
        try:
            reg_model = getattr(model.vars, self._peripheral_name + "_" + reg_field_name)
        except AttributeError:
            raise AttributeError(f"Model {self._peripheral_name} does not have register {reg_field_name}")
        self._reg_write(reg_model, field_value, default, do_not_care, limit_upper,
                   limit_lower, check_saturation, allow_neg, neg_twos_comp)

    def _ip_protected_reg_write(self, model, reg_field_name, field_value=None,
                      default=False, do_not_care=False, limit_upper=None, limit_lower=None,
                      check_saturation=False, allow_neg=False, neg_twos_comp=True):
        """
        This is a thin wrapper around _ip_reg_write: if the current model.target matches
        _protected_reg_field_skip_target, the write is skipped entirely (the field was never
        added to model.vars for that target - see buildVariables), otherwise the write is
        forwarded to _ip_reg_write unchanged.

        :param model: model instance
        :param reg_field_name: protected reg field name, as declared in protected_reg_fields.py
        :param field_value: value to write; see _ip_reg_write for all other params
        """
        if self.should_skip_protected_reg_fields_for_target(model.target):
            LogMgr.Debug(f"Skipping protected reg field {reg_field_name} for target {model.target}")
        else:
            self._ip_reg_write(model, reg_field_name, field_value, default, do_not_care, limit_upper, limit_lower,
                               check_saturation, allow_neg, neg_twos_comp)

    def _ip_protected_reg_write_default(self, model, reg_field_name):
        """
        This is a thin wrapper around _ip_reg_write_default: if the current model.target matches
        _protected_reg_field_skip_target, the write is skipped entirely (the field was never
        added to model.vars for that target - see buildVariables), otherwise the write is
        forwarded to _ip_reg_write_default unchanged.
        """
        if self.should_skip_protected_reg_fields_for_target(model.target):
            LogMgr.Debug(f"Skipping protected reg field {reg_field_name} for target {model.target}")
        else:
            self._ip_reg_write_default(model, reg_field_name)

    def _ip_reg_write_default(self, model, reg_field_name):
        """
        This is a thin wrapper around _reg_write_default that automatically prepends the peripheral name to the
        register field  name, so that the caller can just pass in the register field name without the peripheral name.
        """
        try:
            reg_model = getattr(model.vars, self._peripheral_name + "_" + reg_field_name)
        except AttributeError:
            raise AttributeError(f"Model {self._peripheral_name} does not have register {reg_field_name}")
        self._reg_write_default(reg_model)
