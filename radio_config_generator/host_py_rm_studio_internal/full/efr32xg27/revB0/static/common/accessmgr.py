
"""
Access Manager
^^^^^^^^^^^^^^
This package can raise a number of exceptions from the top-level device class.

  .. autoexception:: AccessMgrIoException

  .. autoclass:: Offline_AccessManager
     :members:
     :show-inheritance:
     :inherited-members:

"""
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
from collections import defaultdict

__all__ = ['AccessMgrIoException', 'Offline_AccessManager']


class AccessMgrIoException(Exception):
    """
    Indicates an error occurred during access of the offline
    register map.
    """
    pass


class Offline_AccessManager(object):
    """
    This class provides the offline register map dictionary for the
    offline access mode.
    """

    def __init__(self, label):
        """Store the label and create the offline register dictionary.

        Args:
            label (str): The DUT label to use in the logger.

        """
        assert isinstance(label, basestring)
        self._label = label
        self._sim_regs = defaultdict(long)

    def ReadRegister(self, address):
        """Read the simulated 32-bit memory mapped value at given address.

        Args:
            address (int|long): The offline 32-bit memory mapped register
                address.

        Returns:
            long: The simulated register value.
        """
        return self._sim_regs[address]

    def WriteRegister(self, address, data):
        """Write the 32-bit data to the memory mapped 32-bit register address.

        Args:
            address (int|long): The offline memory mapped register address.
            data (int|long): The 32-bit data

        Returns:
            long: returns zero on success

        """
        self._sim_regs[address] = data
        return 0

    def Connect(self):
        pass

    def Disconnect(self):
        pass
