from pyradioconfig.protected_fields.base_protected_fields import ProtectedFieldsBase


class ProtectedFieldsLeopard(ProtectedFieldsBase):

    """
    PTE has two types of protected fields
    1) cal
    2) set and forget

    For radio configurator,
    1) Protect all PTE cal fields (do not write)
    2) Ensure set and forget fields are same as PTE if radio configurator is also programming the values

    For rail_script,
    1) protect all PTE cal fields
    2) Protect set and forget field, if register contains pte cal field or radio configurator writes to the field

    """

    PTE_PROTECTED_FIELD_DICT = {
        'RAC_RFBIASCAL_RFBIASCALTC': 26,
        'RAC_RFBIASCAL_RFBIASCALVREF': 'cal',
        'RAC_RFBIASCAL_RFBIASCALBIAS': 'cal',
        'RAC_RFBIASCTRL_RFBIASNONFLASHMODE': 1,
        'RAC_PRECTRL_PREREGTRIM': 'cal',
        'RAC_IFADCCAL_IFADCTUNERC': 'cal',
        'RAC_IFADCTRIM0_IFADCLDOSHUNTAMPLVL1': 1,
        'RAC_IFADCTRIM0_IFADCLDOSHUNTAMPLVL2': 1,
        'RAC_IFADCTRIM0_IFADCLDOSHUNTCURLVL1': 7,
        'RAC_IFADCTRIM0_IFADCLDOSHUNTCURLVL2': 2,
        'RAC_PGACTRL_PGAPOWERMODE': 3,
        'RAC_PGATRIM_PGACTUNE': 'cal',
        'RAC_AUXADCTRIM_AUXADCRCTUNE': 'cal',
        'RAC_AUXADCTRIM_AUXADCLDOVREFTRIM': 2,
        'RAC_AUXADCTRIM_AUXADCTSENSETRIMVBE2': 1,
        'RAC_CLKMULTEN0_CLKMULTREG2ADJV': 0x0,
        'SYNTH_LPFCTRL2TX_LPFINCAPTX': 2, # : set in rail
        'SYNTH_LPFCTRL2TX_CALCTX': 'cal',
        'SYNTH_LPFCTRL2RX_LPFINCAPRX': 2, # : set in rail
        'SYNTH_LPFCTRL2RX_CALCRX': 'cal',
        'SYNTH_VCDACCTRL_VCDACVAL': 'cal',
        'SYNTH_VCOGAIN_VCOKVFINE': 'cal',
        'SYNTH_VCOGAIN_VCOKVCOARSE': 'cal',
        'RAC_LNAMIXTRIM0_LNAMIXRFPKDCALCM': 'cal',
        'RAC_LNAMIXTRIM0_LNAMIXLOWCUR': 'cal',
        'RAC_LNAMIXTRIM1_LNAMIXIBIASADJ': 'cal',
        'RAC_PATRIM0_TX0DBMTRIMDUTYCYP': 'cal',
        'RAC_PATRIM0_TX0DBMTRIMDUTYCYN': 'cal',
        'RAC_PATRIM0_TX0DBMTRIMBIASP': 'cal',
        'RAC_PATRIM0_TX0DBMTRIMBIASN': 'cal',
        'RAC_PATRIM1_TX0DBMTRIMREGVREF': 'cal',
        'RAC_PATRIM1_TX0DBMTRIMREGFB': 'cal',
        'RAC_PATRIM2_TX6DBMTRIMDUTYCYP': 'cal',
        'RAC_PATRIM2_TX6DBMTRIMDUTYCYN': 'cal',
        'RAC_PATRIM2_TX6DBMTRIMBIASP': 'cal',
        'RAC_PATRIM2_TX6DBMTRIMBIASN': 'cal',
        'RAC_PATRIM3_TX6DBMTRIMREGVREF': 'cal',
        'RAC_PATRIM3_TX6DBMTRIMPREDRVREGVREF': 'cal',
        'RAC_PATRIM3_TX6DBMTRIMPREDRVREGFB':0,
        'RAC_PATRIM3_TX6DBMTRIMREGFB': 'cal',
        'MODEM_IRCALCOEFWR0_CRVWD': 'cal',
        'MODEM_IRCALCOEFWR0_CIVWD': 'cal',
        'MODEM_IRCALCOEFWR0_CRVWEN': 1,
        'MODEM_IRCALCOEFWR0_CIVWEN': 1,
        'MODEM_IRCALCOEFWR1_CRVWD': 'cal',
        'MODEM_IRCALCOEFWR1_CIVWD': 'cal',
        'MODEM_IRCALCOEFWR1_CRVWEN': 1,
        'MODEM_IRCALCOEFWR1_CIVWEN': 1,
        'MODEM_IRCAL_MURSHF': 0,
        'MODEM_IRCAL_MUISHF': 0,
        'MODEM_IRCAL_IRCALEN': 1,
        'MODEM_IRCAL_IRCORREN': 1,
        'RAC_SPARE_TX0DBMSPARE': 7,
        'RAC_SPARE_TX6DBMSPARE': 3,
    }

    PTE_PROTECTED_FIELD_EXCEPTIONS = [
        'SYNTH_LPFCTRL2TX_LPFINCAPTX', #: 2,  # : set in rail
        'SYNTH_LPFCTRL2RX_LPFINCAPRX', #: 2,  # : set in rail
        'RAC_PGACAL_PGAOFFPCALQ',#: 'cal',  # : cal by fw
        'RAC_PGACAL_PGAOFFPCALI',#: 'cal',  # : cal by fw
        'RAC_PGACAL_PGAOFFNCALQ',#: 'cal',  # : cal by fw
        'RAC_PGACAL_PGAOFFNCALI',#: 'cal',  # : cal by fw
        'MODEM_IRCALCOEFWR0_CRVWD', # : set by RAIL
        'MODEM_IRCALCOEFWR0_CIVWD', # : set by RAIL
        'MODEM_IRCALCOEFWR1_CRVWD',  # : set by RAIL
        'MODEM_IRCALCOEFWR1_CIVWD',  # : set by RAIL
    ]

    RAIL_PROTECTED_FIELDS = [
        'AGC_CTRL0_RSSISHIFT',
        'AGC_CTRL1_CCATHRSH',
        'FRC_RXCTRL_BUFRESTORERXABORTED',
        'FRC_RXCTRL_BUFRESTOREFRAMEERROR',
        'FRC_RXCTRL_BUFCLEAR',
        'FRC_RXCTRL_TRACKABFRAME',
        'FRC_RXCTRL_ACCEPTBLOCKERRORS',
        'FRC_RXCTRL_ACCEPTCRCERRORS',
        'FRC_RXCTRL_STORECRC',
        'MODEM_IRCALCOEF_CIV',
        'MODEM_IRCALCOEF_CRV',
        'MODEM_IRCAL_MURSHF',
        'MODEM_IRCAL_MUISHF',
        'RAC_SYEN_SYENVCOBIAS',
        'RAC_SYEN_SYENMMDREG',
        'RAC_SYEN_SYLODIVLDOBIASEN',
        'RAC_SYEN_SYENCHPREG',
        'RAC_SYEN_SYENCHPREPLICA',
        'RAC_SYEN_SYENMMDREPLICA1',
        'RAC_SYEN_SYENVCOPFET',
        'RAC_SYEN_SYENVCOREG',
        'RAC_SYEN_SYLODIVLDOEN',
        'RAC_SYEN_SYCHPEN',
        'RAC_SYEN_SYLODIVEN',
        'RAC_AUXADCTRIM_AUXADCLDOVREFTRIM',
        'RAC_PATRIM3_TX6DBMTRIMREGFB',
        'RAC_PATRIM3_TX6DBMTRIMPREDRVREGFB',
        'RAC_IFADCTRIM0_IFADCLDOSHUNTAMPLVL1',
        'RAC_IFADCTRIM0_IFADCLDOSHUNTAMPLVL2',
        'RAC_IFADCTRIM0_IFADCLDOSHUNTCURLVL1',
        'RAC_PGACTRL_PGAPOWERMODE',
        'RAC_CLKMULTEN0_CLKMULTREG2ADJV',
    ]

    RAIL_PROTECTED_FIELD_EXCEPTIONS = [
    ]