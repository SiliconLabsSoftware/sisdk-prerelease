
__all__ = [ 'Base_RM_Device' ]

from collections import OrderedDict
import io
import importlib
import warnings
try:
    unicode = unicode
except NameError:
    str = str
    unicode = str
    bytes = bytes
    basestring = (str,bytes)
    long = int
else:
    str = str
    unicode = unicode
    bytes = str
    basestring = basestring
    long = long
from .. interface import IRegMapDevice
from .. common import Offline_AccessManager, \
    RegisterMapInterface, \
    RegMapAccessError, \
    RegMapNameError, \
    RegMapAddressError, \
    RegMapValueError
from .peripheral import Base_RM_Peripheral
from . register import Base_RM_Register
from . field import Base_RM_Field

class Base_RM_Device(IRegMapDevice):

    def __init__(self, rmio, label, name, svd_info):
        self.__dict__['zz_frozen'] = False
        self.zz_name = name
        self.zz_label = label
        self.zz_svd_info = svd_info
        if rmio is None:
            self.offline = True
            accessMgr = Offline_AccessManager(self.zz_label)
            self.zz_rmio = RegisterMapInterface(accessMgr.ReadRegister,
                                                accessMgr.WriteRegister,
                                                simulated=True)
        else:
            self.offline = False
            self.zz_rmio = rmio
        self.zz_pdict = OrderedDict()
        self.zz_reg_addr_to_name = {}
        self.zz_reg_addr_to_names = {}
        self.zz_dump_cb_functions = []

    def __setattr__(self, name, value):
        if self.__dict__['zz_frozen']:
            if name not in self.zz_pdict:
                raise AttributeError("ERROR: Invalid peripheral '{}'\n".format(name))
            else:
                raise AttributeError("ERROR: Unable to set '{}' to '{}'\n".format(name, value))
        else:
            self.__dict__[name] = value

    def __iter__(self):
        self._load_all_peripherals()
        for key in self.zz_pdict:
            yield self.zz_pdict[key]

    def __repr__(self):
        peripheral_names = self._get_declared_peripheral_names()
        if peripheral_names:
            keys = sorted(peripheral_names)
        else:
            keys = sorted(self.zz_pdict)
        out = "{} ({} peripherals)\n".format(self.zz_name, len(keys))
        for key in keys:
            out += "  {}\n".format(key)
        return out

    def __dir__(self):
        return sorted(set(object.__dir__(self)) |
                      set(self._get_declared_peripheral_names()) |
                      set(self.__dict__.get('zz_alias_to_per_name', {})))

    def _get_declared_peripheral_names(self):
        names = self.__dict__.get('zz_peripheral_names')
        if names is None:
            return tuple(self.__dict__.get('zz_pdict', {}).keys())
        return tuple(names)

    def _get_declared_peripheral_name_set(self):
        name_set = self.__dict__.get('zz_peripheral_name_set')
        if name_set is None:
            return frozenset(self._get_declared_peripheral_names())
        return name_set

    def _load_peripheral(self, peripheral_name):
        requested_name = peripheral_name
        alias_map = self.__dict__.get('zz_alias_to_per_name', {})
        if peripheral_name in alias_map:
            peripheral_name = alias_map[peripheral_name]
        try:
            peripheral = self.zz_pdict[peripheral_name]
            if requested_name != peripheral_name:
                self.__dict__[requested_name] = peripheral
            return peripheral
        except KeyError:
            pass
        if peripheral_name not in self._get_declared_peripheral_name_set():
            raise KeyError(peripheral_name)
        class_module = importlib.import_module(self.__class__.__module__)
        module = importlib.import_module('.' + peripheral_name, class_module.__package__)
        peripheral_class = getattr(module, 'RM_Peripheral_' + peripheral_name)
        peripheral = peripheral_class(self.zz_rmio, self.zz_label)
        self.zz_pdict[peripheral_name] = peripheral
        self.__dict__[peripheral_name] = peripheral
        if self.offline:
            peripheral.assignRegDefault()
        peripheral.getAddressNameMap(self.zz_reg_addr_to_name)
        peripheral.getAddressNamesMap(self.zz_reg_addr_to_names)
        if peripheral.isAliased():
            peripheral.registerAddressToNamesCb(self.addressToNames)
        for alias_name in self.__dict__.get('zz_per_to_alias_names', {}).get(peripheral_name, ()):
            self.__dict__[alias_name] = peripheral
        return peripheral

    def _load_all_peripherals(self):
        if self.__dict__.get('zz_loaded_all_peripherals', False):
            return
        for peripheral_name in self._get_declared_peripheral_names():
            self._load_peripheral(peripheral_name)
        self.__dict__['zz_loaded_all_peripherals'] = True

    def __getattr__(self, name):
        if name.startswith('__') and name.endswith('__'):
            raise AttributeError(name)
        if name in self._get_declared_peripheral_name_set() or name in self.__dict__.get('zz_alias_to_per_name', {}):
            return self._load_peripheral(name)
        raise AttributeError("ERROR: Invalid peripheral '{}'\n".format(name))

    @property
    def svdInfo(self):
        return self.zz_svd_info

    def addressToName(self, address):
        self._load_all_peripherals()
        warnings.warn("addressToName() is deprecated; use addressToNames() for aliased register support",
                      DeprecationWarning, stacklevel=2)
        if not isinstance(address, (int, long)):
            raise RegMapAddressError("Invalid address '{}'".format(address))
        try:
            return self.zz_reg_addr_to_name[address]
        except KeyError:
            raise RegMapAddressError("No register found for address {:#010x}".format(address))

    def addressToNames(self, address):
        self._load_all_peripherals()
        if not isinstance(address, (int, long)):
            raise RegMapAddressError("Invalid address '{}'".format(address))
        try:
            return self.zz_reg_addr_to_names[address]
        except KeyError:
            raise RegMapAddressError("No registers found for address {:#010x}".format(address))

    def updateAddressToNameCache(self, peripheral):
        """TODO: Remove the update of legacy dict once the addressToName() is removed."""
        if not isinstance(peripheral, Base_RM_Peripheral):
            raise RegMapNameError("Invalid peripheral attribute {}".format(peripheral))
        for addr in list(self.zz_reg_addr_to_name.keys()):
            fullname = self.zz_reg_addr_to_name[addr]
            if fullname.split('.')[0] == peripheral.name:
                del self.zz_reg_addr_to_name[addr]
        for reg_name in peripheral.zz_rdict:
            reg = peripheral.zz_rdict[reg_name]
            self.zz_reg_addr_to_name[reg.baseAddress + reg.addressOffset] = reg.fullname
        for addr in list(self.zz_reg_addr_to_names.keys()):
            self.zz_reg_addr_to_names[addr] = [
                fn for fn in self.zz_reg_addr_to_names[addr]
                if fn.split('.')[0] != peripheral.name
            ]
        for reg_name in peripheral.zz_rdict:
            reg = peripheral.zz_rdict[reg_name]
            addr = reg.baseAddress + reg.addressOffset
            if addr not in self.zz_reg_addr_to_names:
                self.zz_reg_addr_to_names[addr] = [reg.fullname]
            elif reg.fullname not in self.zz_reg_addr_to_names[addr]:
                self.zz_reg_addr_to_names[addr].append(reg.fullname)

    def nameToAddress(self, name):
        reg_or_field = self.getObjectByName(name)
        return reg_or_field.baseAddress + reg_or_field.addressOffset

    def writeByName(self, name, value):
        if not isinstance(value, (int, long)):
            raise RegMapValueError("Invalid value {} for '{}'".format(value, name))
        self.getObjectByName(name).io = value

    def readByName(self, name):
        return self.getObjectByName(name).io

    def getObjectByName(self, name):
        if not isinstance(name, basestring):
            raise RegMapNameError("Invalid name '{}' must be str".format(name))
        items = name.split('.')
        try:
            peripheral = self._load_peripheral(items[0])
        except KeyError:
            raise RegMapNameError("Invalid peripheral name '{}'".format(name))
        if len(items) == 1:
            return peripheral
        elif len(items) == 2:
            try:
                return peripheral.zz_rdict[items[1]]
            except KeyError:
                raise RegMapNameError("Invalid register name '{}'".format(name))
        elif len(items) == 3:
            try:
                return peripheral.zz_rdict[items[1]].zz_fdict[items[2]]
            except KeyError:
                raise RegMapNameError("Invalid register field name '{}'".format(name))
        else:
            raise RegMapNameError("Invalid name '{}', must be PER, PER.REG or PER.REG.FIELD".format(name))

    def isReadable(self, name):
        obj = self.getObjectByName(name)
        return obj.isReadable()

    def isWriteable(self, name):
        obj = self.getObjectByName(name)
        return obj.isWriteable()

    def getRegisterNameFromFieldName(self, name):
        periods = len(name.split('.'))
        if periods == 2 or periods == 3:
            return '.'.join(name.split('.')[:2])
        else:
            raise RegMapNameError("Invalid name '{}', must be PER.REG or PER.REG.FIELD".format(name))

    def getPeripheralNames(self):
        return sorted(self._get_declared_peripheral_names())

    def getRegisterNames(self):
        self._load_all_peripherals()
        nameList = []
        for key in sorted(self.zz_pdict):
            nameList.extend(self.zz_pdict[key].getRegisterNames())
        return nameList

    def forceRegister(self, name, value):
        if not self.offline:
            raise RegMapAccessError("Cannot directly assign within a live connection")
        reg = self.getObjectByName(name)
        if not isinstance(reg, Base_RM_Register):
            raise RegMapNameError("Name must be register name")
        self.zz_rmio.forceRegister(reg, value)

    def enableAccessedFlags(self):
        self.zz_rmio.enableAccessedFlags()

    def disableAccessedFlags(self):
        self.zz_rmio.disableAccessedFlags()

    def clearAccessedFlags(self):
        self._load_all_peripherals()
        for key in sorted(self.zz_pdict):
            self.zz_pdict[key].clearAccessedFlags()

    def setAccessedFlags(self):
        self._load_all_peripherals()
        for key in sorted(self.zz_pdict):
            self.zz_pdict[key].setAccessedFlags()

    def getAccessedRegisterNames(self):
        self._load_all_peripherals()
        nameList = []
        perObjIdList = []
        for key in sorted(self.zz_pdict):
            if id(self.zz_pdict[key]) not in perObjIdList:
                perObjIdList.append(id(self.zz_pdict[key]))
                nameList.extend(self.zz_pdict[key].getAccessedRegisterNames())
        return nameList

    def getAccessedFieldNames(self):
        self._load_all_peripherals()
        nameList = []
        perObjIdList = []
        for key in sorted(self.zz_pdict):
            if id(self.zz_pdict[key]) not in perObjIdList:
                perObjIdList.append(id(self.zz_pdict[key]))
                nameList.extend(self.zz_pdict[key].getAccessedFieldNames())
        return nameList

    @staticmethod
    def convertListToOrderedDict(inList, defaultValue=0):
        return OrderedDict([ (key, defaultValue) for key in inList])

    def writeData(self, dataDict):
        for key in dataDict:
            self.writeByName(key, dataDict[key])

    def readData(self, dataDict):
        for key in dataDict:
            dataDict[key] = self.readByName(key)

    def verifyData(self, expectedDict):
        diffDict = OrderedDict()
        for key in expectedDict:
            actualValue= self.readByName(key)
            if actualValue != expectedDict[key]:
                diffDict[key] = actualValue
        return diffDict

    def readAccessedRegisters(self):
        valueDict = OrderedDict()
        for key in self.getAccessedRegisterNames():
            valueDict[key] = self.readByName(key)
        return valueDict

    def readAccessedFields(self):
        valueDict = OrderedDict()
        for key in self.getAccessedFieldNames():
            valueDict[key] = self.readByName(key)
        return valueDict

    def getExcludedDumpRegisterNames(self):
        self._load_all_peripherals()
        nameList = []
        for key in sorted(self.zz_pdict):
            nameList.extend(self.zz_pdict[key].getExcludedDumpRegisterNames())
        return nameList

    def excludeFromDumpByName(self, nameList):
        for name in nameList:
            obj = self.getObjectByName(name)
            try:
                obj.excludeFromDump()
            except AttributeError:
                raise RegMapNameError("Invalid name '{}', please use register or field name".format(name))

    def includeInDump(self):
        self._load_all_peripherals()
        for key in self.zz_pdict:
            self.zz_pdict[key].includeInDump()

    def appendDumpDescCallback(self, cb_function):
        if not callable(cb_function):
            raise ValueError("'{}' is not callable".format(cb_function))
        self.zz_dump_cb_functions.append(cb_function)

    def clearDumpDescCallbacks(self):
        self.__dict__['zz_dump_cb_functions'] = []

    def buildRegFilterList(self, filename, listname='regFilterList'):
        self._load_all_peripherals()
        filterList = []
        with io.open(filename, mode='w', encoding='utf-8') as outFH:
            outFH.write(u"\n# -*- coding: utf-8 -*- \n")
            outFH.write(u"\n# Register Map Register Name Filter List\n")
            outFH.write(u"\n{} = [\n".format(listname))
            for key in sorted(self.zz_pdict):
                self.zz_pdict[key].buildRegFilterList(outFH, filterList)
            outFH.write(u"]\n\n")
        return filterList

    def dump(self, filename, regFilterList=None, userMsg='', ignoreFailures=False):
        self._load_all_peripherals()
        valueDict = OrderedDict()
        with io.open(filename, mode='w', encoding='utf-8') as outFH:
            outFH.write(u"\n# -*- coding: utf-8 -*- \n")
            for cb_function in self.zz_dump_cb_functions:
                desc = cb_function()
                for line in desc.split('\n'):
                    if line == '' or line.startswith('#'):
                        out_line = line + '\n'
                    else:
                        out_line = '# ' + line + '\n'
                    outFH.write(unicode(out_line))

            if userMsg:
                for line in userMsg.split('\n'):
                    if not line.startswith('#'):
                        outFH.write(u"# {}\n".format(line))
                    else:
                        outFH.write(u"{}\n".format(line))

            outFH.write(u"\nfrom collections import OrderedDict\n")
            outFH.write(u"\nREGISTER_DUMP = OrderedDict([\n")
            if regFilterList:
                for name in regFilterList:
                    obj = self.getObjectByName(name)
                    if isinstance(obj, Base_RM_Register):
                        obj.dump(outFH, valueDict, ignoreFailures=ignoreFailures)
                    elif isinstance(obj, Base_RM_Field):
                        obj = self.getObjectByName(self.getRegisterNameFromFieldName(name))
                        field_name = name.split('.')[-1]
                        obj.dump_field(outFH, valueDict, field_name, ignoreFailures=ignoreFailures)
                    else:
                        outFH.write(u"    # Skipping invalid name '{}'\n".format(name))
            else:
                for key in sorted(self.zz_pdict):
                    self.zz_pdict[key].dump(outFH, valueDict, ignoreFailures=ignoreFailures)
            outFH.write(u"])\n\n")
        return valueDict

    def openCLangLogger(self, logger_fn, logger_header='',
                        logger_footer='', logger_label='rm_c_source'):
        self.zz_rmio.openCLangLogger(logger_fn, logger_header, logger_footer, logger_label)

    def enableCLangLogger(self):
        self.zz_rmio.enableCLangLogger()

    def disableCLangLogger(self):
        self.zz_rmio.disableCLangLogger()

    def closeCLangLogger(self):
        self.zz_rmio.closeCLangLogger()

    def openPyLangLogger(self, logger_fn, rm_obj_prefix='dut.rm.',
                         logger_header='', logger_footer='',
                         logger_label='rm_py_source'):
        self.zz_rmio.openPyLangLogger(logger_fn, rm_obj_prefix, logger_header,
                                      logger_footer, logger_label)

    def enablePyLangLogger(self):
        self.zz_rmio.enablePyLangLogger()

    def disablePyLangLogger(self):
        self.zz_rmio.disablePyLangLogger()

    def closePyLangLogger(self):
        self.zz_rmio.closePyLangLogger()

    def assignRawWordBlock(self, address, wordList):
        self._load_all_peripherals()
        self.zz_rmio.assignRawWordBlock(address, wordList)
