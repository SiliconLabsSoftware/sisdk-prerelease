

from . static import Base_RM_Field


class RM_Field_PROPOQPSK_CFG1_MODULATION(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_CFG1_MODULATION, self).__init__(register,
            'MODULATION', 'PROPOQPSK.CFG1.MODULATION', 'read-write',
            u"",
            0, 8)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_CFG1_CHIPRATE(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_CFG1_CHIPRATE, self).__init__(register,
            'CHIPRATE', 'PROPOQPSK.CFG1.CHIPRATE', 'read-write',
            u"",
            8, 8)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_CFG1_BANDFREQMHZ(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_CFG1_BANDFREQMHZ, self).__init__(register,
            'BANDFREQMHZ', 'PROPOQPSK.CFG1.BANDFREQMHZ', 'read-write',
            u"",
            16, 16)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_CFG2_MACFCSTYPE(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_CFG2_MACFCSTYPE, self).__init__(register,
            'MACFCSTYPE', 'PROPOQPSK.CFG2.MACFCSTYPE', 'read-write',
            u"",
            0, 1)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_CFG2_NBPREAMB(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_CFG2_NBPREAMB, self).__init__(register,
            'NBPREAMB', 'PROPOQPSK.CFG2.NBPREAMB', 'read-write',
            u"",
            8, 8)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_CFG2_XTALFREQ(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_CFG2_XTALFREQ, self).__init__(register,
            'XTALFREQ', 'PROPOQPSK.CFG2.XTALFREQ', 'read-write',
            u"",
            16, 16)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_VREGS_MINORVERSION(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_VREGS_MINORVERSION, self).__init__(register,
            'MINORVERSION', 'PROPOQPSK.VREGS.MINORVERSION', 'read-write',
            u"",
            16, 8)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_VREGS_MAJORVERSION(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_VREGS_MAJORVERSION, self).__init__(register,
            'MAJORVERSION', 'PROPOQPSK.VREGS.MAJORVERSION', 'read-write',
            u"",
            24, 8)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_COLLDET_COLLDET(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_COLLDET_COLLDET, self).__init__(register,
            'COLLDET', 'PROPOQPSK.COLLDET.COLLDET', 'read-write',
            u"",
            0, 1)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_COLLDET_COLLDETTHR(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_COLLDET_COLLDETTHR, self).__init__(register,
            'COLLDETTHR', 'PROPOQPSK.COLLDET.COLLDETTHR', 'read-write',
            u"",
            1, 5)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_AFC_AFCMIXERRATIOGAIN(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_AFC_AFCMIXERRATIOGAIN, self).__init__(register,
            'AFCMIXERRATIOGAIN', 'PROPOQPSK.AFC.AFCMIXERRATIOGAIN', 'read-write',
            u"",
            0, 16)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_AFC_AFCMIXERRATIOATT(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_AFC_AFCMIXERRATIOATT, self).__init__(register,
            'AFCMIXERRATIOATT', 'PROPOQPSK.AFC.AFCMIXERRATIOATT', 'read-write',
            u"",
            16, 5)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_AFC_AFCFBITER(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_AFC_AFCFBITER, self).__init__(register,
            'AFCFBITER', 'PROPOQPSK.AFC.AFCFBITER', 'read-write',
            u"",
            24, 4)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_AFC_AFCCORRECTION(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_AFC_AFCCORRECTION, self).__init__(register,
            'AFCCORRECTION', 'PROPOQPSK.AFC.AFCCORRECTION', 'read-write',
            u"",
            30, 2)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_TXCFG1_TXHCSERRORS(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_TXCFG1_TXHCSERRORS, self).__init__(register,
            'TXHCSERRORS', 'PROPOQPSK.TXCFG1.TXHCSERRORS', 'read-write',
            u"",
            1, 1)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_TXCFG1_TXFCSERRORS(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_TXCFG1_TXFCSERRORS, self).__init__(register,
            'TXFCSERRORS', 'PROPOQPSK.TXCFG1.TXFCSERRORS', 'read-write',
            u"",
            2, 1)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_TXCFG1_TX_IQ_FILTER(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_TXCFG1_TX_IQ_FILTER, self).__init__(register,
            'TX_IQ_FILTER', 'PROPOQPSK.TXCFG1.TX_IQ_FILTER', 'read-write',
            u"",
            28, 4)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_ANT_ANTDIV(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_ANT_ANTDIV, self).__init__(register,
            'ANTDIV', 'PROPOQPSK.ANT.ANTDIV', 'read-write',
            u"",
            0, 1)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_ANT_ANTSEL(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_ANT_ANTSEL, self).__init__(register,
            'ANTSEL', 'PROPOQPSK.ANT.ANTSEL', 'read-write',
            u"",
            1, 2)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_PDET_NBHITSDET(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_PDET_NBHITSDET, self).__init__(register,
            'NBHITSDET', 'PROPOQPSK.PDET.NBHITSDET', 'read-write',
            u"",
            0, 8)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_PDET_NSPDET(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_PDET_NSPDET, self).__init__(register,
            'NSPDET', 'PROPOQPSK.PDET.NSPDET', 'read-write',
            u"",
            8, 8)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_PDET_NBCHUNKS(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_PDET_NBCHUNKS, self).__init__(register,
            'NBCHUNKS', 'PROPOQPSK.PDET.NBCHUNKS', 'read-write',
            u"",
            16, 8)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_PDTHR_MINTHR(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_PDTHR_MINTHR, self).__init__(register,
            'MINTHR', 'PROPOQPSK.PDTHR.MINTHR', 'read-write',
            u"",
            0, 16)
        self.__dict__['zz_frozen'] = True


class RM_Field_PROPOQPSK_PDTHR_TSRATIO(Base_RM_Field):
    def __init__(self, register):
        self.__dict__['zz_frozen'] = False
        super(RM_Field_PROPOQPSK_PDTHR_TSRATIO, self).__init__(register,
            'TSRATIO', 'PROPOQPSK.PDTHR.TSRATIO', 'read-write',
            u"",
            16, 16)
        self.__dict__['zz_frozen'] = True


