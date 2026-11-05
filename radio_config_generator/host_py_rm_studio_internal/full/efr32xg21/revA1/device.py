

"""
Device
^^^^^^
This package provides a top-level device class, ``RM_Device_EFR32X21X000F1024XM32_RevA1``,
which is instantiated in either an offline mode for stand-alone simulated
register I/O or in a live session with a real part.

  .. autoclass::  RM_Device_EFR32X21X000F1024XM32_RevA1
     :members:
     :show-inheritance:
     :inherited-members:

The ARM CMSIS System View Description (SVD) XML hierarchy is reflected in
this package module composition. Peripheral modules are loaded lazily on first
access while static typing hints are provided by the companion ``device.pyi``.

"""


__all__ = [ 'RM_Device_EFR32X21X000F1024XM32_RevA1' ]

from . static import Base_RM_Device, RM_SVD_Info
from .... common import RM_TrustZone_Access_Mode

import importlib
from pathlib import Path

_PERIPHERAL_NAMES = (
    'AGC_NS',
    'AGC_S',
    'FRC_NS',
    'FRC_S',
    'MODEM_NS',
    'MODEM_S',
    'RAC_NS',
    'RAC_S',
    'RFCRC_NS',
    'RFCRC_S',
    'SEQ',
    'SYNTH_NS',
    'SYNTH_S',
)

_PERIPHERAL_NAME_SET = frozenset(_PERIPHERAL_NAMES)


def _build_default_aliases(peripherals, mode='secure'):
    """Build default TrustZone aliases from peripheral naming conventions."""
    aliases = {}
    peripherals_set = set(peripherals)

    for peripheral in peripherals:
        if mode == 'secure':
            if peripheral.endswith('_S'):
                alias_name = peripheral[:-2]
                if alias_name + '_NS' in peripherals_set:
                    aliases[alias_name] = peripheral
            elif '_S_' in peripheral:
                alias_name = peripheral.replace('_S_', '_')
                nonsecure_name = peripheral.replace('_S_', '_NS_')
                if nonsecure_name in peripherals_set:
                    aliases[alias_name] = peripheral
        else:
            if peripheral.endswith('_NS'):
                alias_name = peripheral[:-3]
                if alias_name + '_S' in peripherals_set:
                    aliases[alias_name] = peripheral
            elif '_NS_' in peripheral:
                alias_name = peripheral.replace('_NS_', '_')
                secure_name = peripheral.replace('_NS_', '_S_')
                if secure_name in peripherals_set:
                    aliases[alias_name] = peripheral

    return aliases


def _build_reverse_alias_map(alias_map):
    reverse_map = {}
    for alias_name, peripheral_name in alias_map.items():
        reverse_map.setdefault(peripheral_name, []).append(alias_name)
    return {key: tuple(sorted(value)) for key, value in reverse_map.items()}


_PKG_PATH = Path(__file__).resolve().parent
_EXCLUDED_REGS_MOD_NAME = 'excluded_regs'
_EXCLUDED_REGS_LIST_NAME = 'EXCLUDED_REGS'


class RM_Device_EFR32X21X000F1024XM32_RevA1(Base_RM_Device):
    """
    The top level register map class. This class can be used in an offline
    mode for stand-alone simulated register I/O. In this case the class will
    use a dictionary initialized with reset values from the CMSIS SVD XML.

    .. code-block:: py

        import sys
        sys.path.append("path/to/this/package")

        from host_py_rm_studio_internal import RM_Factory
        rm_device_class = RM_Factory("JUMBO", "A0")
        rm = rm_device_class(label='offline_rm')

    The class is used by the base application classes in the Python Debug Bench
    to control the part. This requires an RPC interface provided by the
    pyvsrpc package. If you want to use live outside the PDB, you can use the
    JLINK_AccessManager class from the pyjlinklib package.

    .. code-block:: py

        import sys
        sys.path.append("path/to/the/packages")

        from pyrmsvd import *
        from pyjlinklib import *
        from host_py_rm_studio_internal import RM_Factory

        dut_label = 'dut_1'
        accessMgr = JLINK_AccessManager(dut_label, JLINK_ARM_OPTIONS(), False)
        accessMgr.Connect()

        hwinfo = EFRDetectPlatform(self.accessMgr)
        rm_device_class = RM_Factory(hwinfo.family_name, hwinfo.rev_name)
        rmIO = RegisterMapInterface(accessMgr.ReadRegister,
                                    accessMgr.WriteRegister)
        rm = rm_device_class(rmIO, dut_label)

    The ``svdInfo`` property is an instance of :class:`pyrmsvd.static.common.svdinfo.RM_SVD_Info`.

    """

    def __init__(self, rmio=None, label='offline_rm', default_tz_access_mode=RM_TrustZone_Access_Mode.TZ_SECURE):
        """
        Register the ``RegisterMapInterface`` instance and the logging label.

        :type  rmio: :class:`pyrmsvd.static.common.regmapio.RegisterMapInterface` or ``NoneType``
        :param rmio: The register map I/O instance, which contains the \
                     read/write functions for the registers. If ``None``, then \
                     the class will automatically create an internal ``RegisterMapInterface`` \
                     with an :class:`pyrmsvd.static.common.accessmgr.Offline_AccessManager` \
                     instance for operation in offline mode.
        :type  label: ``str``
        :param label: The register map DUT label to use in the system logger.
        :param default_tz_access_mode: Default ARM v8m Trustzone access mode to be used \
        			for non-suffixed peripherals ('S' = Secure; 'NS' = Non-Secure). This \
        			option only applies to Series 2/3 devices with Cortex M33 core
        """
        assert isinstance(default_tz_access_mode, RM_TrustZone_Access_Mode)
        self.__dict__['zz_frozen'] = False
        super(RM_Device_EFR32X21X000F1024XM32_RevA1, self).__init__(rmio, label,
            'EFR32X21X000F1024XM32',
            RM_SVD_Info('EFR32X21X000F1024XM32_SEQ.svd', 'f41ee51d6f03a649d7990bae0f838126'))

        self.__dict__['zz_peripheral_names'] = _PERIPHERAL_NAMES
        self.__dict__['zz_peripheral_name_set'] = _PERIPHERAL_NAME_SET
        self.__dict__['zz_loaded_all_peripherals'] = False
        secure_aliases = _build_default_aliases(_PERIPHERAL_NAMES, mode='secure')
        nonsecure_aliases = _build_default_aliases(_PERIPHERAL_NAMES, mode='nonsecure')
        if default_tz_access_mode == RM_TrustZone_Access_Mode.TZ_SECURE:
            self.__dict__['zz_alias_to_per_name'] = secure_aliases
            self.__dict__['zz_per_to_alias_names'] = _build_reverse_alias_map(secure_aliases)
        else:
            self.__dict__['zz_alias_to_per_name'] = nonsecure_aliases
            self.__dict__['zz_per_to_alias_names'] = _build_reverse_alias_map(nonsecure_aliases)

        self.__dict__['zz_frozen'] = True


        file_name = _PKG_PATH / _EXCLUDED_REGS_MOD_NAME
        file_path_py = file_name.with_suffix('.py')
        file_path_pyc = file_name.with_suffix('.pyc')
        if file_path_py.exists():
            excluded_regs_fn = file_path_py
        elif file_path_pyc.exists():
            excluded_regs_fn = file_path_pyc
        else:
            excluded_regs_fn = None

        if excluded_regs_fn and hasattr(self, 'excludeFromDumpByName'):
            spec = importlib.util.spec_from_file_location(_EXCLUDED_REGS_MOD_NAME, str(excluded_regs_fn))
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            excluded_regs = getattr(mod, _EXCLUDED_REGS_LIST_NAME)
            self.excludeFromDumpByName(excluded_regs)