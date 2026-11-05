"""
This provides values specific to image rejection calibration (ir cal).

"""

"""
Calculator functions are pulled by using their names.
Calculator functions must start with "calc_", if they are to be consumed by the framework.
    Or they should be returned by overriding the function:
        def getCalculationList(self):
"""

import sys
from enum import Enum
from pyradioconfig.calculator_model_framework.interfaces.icalculator import ICalculator
from pycalcmodel.core.variable import ModelVariableFormat, CreateModelVariableEnum
from pyradioconfig.calculator_model_framework.Utils.CustomExceptions import CalculationException

from py_2_and_3_compatibility import *


class CALC_IrCal(ICalculator):

    """
    Init internal variables
    """
    def __init__(self):

        self.sw_useSwRssiAveraging   = True
        self.sw_numRssiToAvg         = 6 # = 2^*6* = 64
        self.sw_throwAwayBeforeRssi  = 0
        self.sw_delayUsBeforeRssi    = 10000
        self.sw_delayUsBetweenSwRssi = 0
        self.hw_useSwRssiAveraging   = False
        self.hw_numRssiToAvg         = 2 # = 2^*2* = 4
        self.hw_throwAwayBeforeRssi  = 2
        self.hw_delayUsBeforeRssi    = 0
        self.hw_delayUsBetweenSwRssi = 0


    def buildVariables(self, model):
        """Populates a list of needed variables for this calculator

        Args:
            model (ModelRoot) : Builds the variables specific to this calculator
        """

        """
        """

        """
        """
        self._addModelVariable(model, 'ircal_auxndiv', int, ModelVariableFormat.DECIMAL, units='bytes', desc='This value is predetermined.')
        self._addModelVariable(model, 'ircal_auxlodiv', int, ModelVariableFormat.DECIMAL, units='bytes', desc='This value is predetermined.')
        self._addModelVariable(model, 'ircal_rampval', int, ModelVariableFormat.DECIMAL, units='bytes', desc='This value is predetermined.')
        self._addModelVariable(model, 'ircal_rxamppll', int, ModelVariableFormat.DECIMAL, units='bytes', desc='This value is predetermined.')
        self._addModelVariable(model, 'ircal_rxamppa', int, ModelVariableFormat.DECIMAL, units='bytes', desc='This value is predetermined.')
        
        self._addModelVariable(model, 'ircal_manufconfigvalid', bool, ModelVariableFormat.ASCII, 'True = the manufacturing calibration value is saved on the chip')
        self._addModelVariable(model, 'ircal_pllconfigvalid', bool, ModelVariableFormat.ASCII, 'True = PLL loopback is permitted to generate a calibration value')
        self._addModelVariable(model, 'ircal_paconfigvalid', bool, ModelVariableFormat.ASCII, 'True = PA loopback is permitted to generate a calibration value')
        var = self._addModelVariable(model, 'ircal_bestconfig', Enum, ModelVariableFormat.DECIMAL, 'Specify the best calibration method for this radio configuration.')
        member_data = [
            ['MANUFACTURING',  1, 'Use the calibration value saved during manufacturing, if applicable.'],
            ['PLL',  2, 'Put the part into a PLL loopback to generate a calibration value.'],
            ['PA',  3, 'Put the part into a PA loopback to generate a calibration value.'],
            ['UNSUPPORTED',  4, 'Image rejection calibration not supported.'],
        ]
        var.var_enum = CreateModelVariableEnum(
            'configType',
            'Specify how image rejection calibration is to run.',
            member_data)

        self._addModelVariable(model, 'ircal_useswrssiaveraging', bool, ModelVariableFormat.ASCII, 'True = use software RSSI averaging; False = use hardware RSSI averaging')
        self._addModelVariable(model, 'ircal_numrssitoavg', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Number of RSSI values (2^value) to average in software. If value = 3, 8 values will be averaged.')
        self._addModelVariable(model, 'ircal_throwawaybeforerssi', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Number of RSSI values to discard before starting to average RSSI values.')
        self._addModelVariable(model, 'ircal_delayusbeforerssi', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Microsecond delay between applying a calibration value and then reading RSSI values.')
        self._addModelVariable(model, 'ircal_delayusbetweenswrssi', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Microsecond delay between gathering RSSI values. Software RSSI averaging mode only.')

        self._addModelVariable(model, 'ircal_agcrssiperiod', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Number of raw RSSI values averaged by hardware.')

        self._addModelVariable(model, 'ircal_useswrssiaveraging2', bool, ModelVariableFormat.ASCII, 'True = use software RSSI averaging; False = use hardware RSSI averaging; Jumbo support')
        self._addModelVariable(model, 'ircal_numrssitoavg2', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Number of RSSI values (2^value) to average in software. If value = 3, 8 values will be averaged. Jumbo support')
        self._addModelVariable(model, 'ircal_throwawaybeforerssi2', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Number of RSSI values to discard before starting to average RSSI values. Jumbo support')
        self._addModelVariable(model, 'ircal_delayusbeforerssi2', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Microsecond delay between applying a calibration value and then reading RSSI values. Jumbo support')
        self._addModelVariable(model, 'ircal_delayusbetweenswrssi2', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Microsecond delay between gathering RSSI values. Software RSSI averaging mode only. Jumbo support')

        var = self._addModelVariable(model, 'ircal_rxtx_path_common', Enum, ModelVariableFormat.DECIMAL, 'RX and TX are on a common/shared circuit, or split. Refer to document AN971.')
        member_data = [
            ['SHARED_RX_TX_PATH' , 0, 'RX and TX circuit paths are common/shared/connected'],
            ['SPLIT_RX_TX_PATH', 1, 'RX and TX circuit paths are separated/not connected'],
        ]
        var.var_enum = CreateModelVariableEnum(
            'IRCalRXTXPathCommonEnum',
            'RX and TX are on a common/shared circuit, or split. Refer to document AN971.',
            member_data)

        self._addModelVariable(model, 'ircal_power_level', int, ModelVariableFormat.DECIMAL, units='codes', desc='Specify IR cal power level (amplitude) instead of auto (0). Refer to document AN971.')

    def calc_ircal_datarate_considerations(self, model):
        bitrate = model.vars.bitrate.value


        if bitrate >= 10000:
            model.vars.ircal_useswrssiaveraging.value = self.sw_useSwRssiAveraging
            model.vars.ircal_numrssitoavg.value = self.sw_numRssiToAvg
            model.vars.ircal_throwawaybeforerssi.value = self.sw_throwAwayBeforeRssi
            model.vars.ircal_delayusbeforerssi.value = self.sw_delayUsBeforeRssi
            model.vars.ircal_delayusbetweenswrssi.value = self.sw_delayUsBetweenSwRssi
        else:
            model.vars.ircal_useswrssiaveraging.value = self.hw_useSwRssiAveraging
            model.vars.ircal_numrssitoavg.value = self.hw_numRssiToAvg
            model.vars.ircal_throwawaybeforerssi.value = self.hw_throwAwayBeforeRssi
            model.vars.ircal_delayusbeforerssi.value = self.hw_delayUsBeforeRssi
            model.vars.ircal_delayusbetweenswrssi.value = self.hw_delayUsBetweenSwRssi


        model.vars.ircal_useswrssiaveraging2.value = self.hw_useSwRssiAveraging
        model.vars.ircal_delayusbeforerssi2.value = self.hw_delayUsBeforeRssi
        model.vars.ircal_delayusbetweenswrssi2.value = self.hw_delayUsBetweenSwRssi

        if bitrate >= 1000000:  # datarates >= 1 Mbps
            model.vars.ircal_numrssitoavg2.value = 0
            model.vars.ircal_throwawaybeforerssi2.value = 0
            model.vars.ircal_agcrssiperiod.value = 12
        elif bitrate >= 500000: # datarates >= 100 Kbps
            model.vars.ircal_numrssitoavg2.value = 0
            model.vars.ircal_throwawaybeforerssi2.value = 0
            model.vars.ircal_agcrssiperiod.value = 10
        elif bitrate >= 100000: # datarates >= 100 Kbps
            model.vars.ircal_numrssitoavg2.value = 0
            model.vars.ircal_throwawaybeforerssi2.value = 0
            model.vars.ircal_agcrssiperiod.value = 9
        elif bitrate >= 10000:  # datarates >= 10 Kbps
            model.vars.ircal_numrssitoavg2.value = 1
            model.vars.ircal_throwawaybeforerssi2.value = 1
            model.vars.ircal_agcrssiperiod.value = 5
        elif bitrate >= 1000:   # datarates >= 1 Kbps
            model.vars.ircal_numrssitoavg2.value = 1
            model.vars.ircal_throwawaybeforerssi2.value = 1
            model.vars.ircal_agcrssiperiod.value = 5
        else:
            model.vars.ircal_numrssitoavg2.value = 1     # 2^1 = really 2
            model.vars.ircal_throwawaybeforerssi2.value = 1
            model.vars.ircal_agcrssiperiod.value = 3    # 2^3 = really 8


    def calc_ircal_common_values(self, model):
        model.vars.ircal_rampval.value = 6
        model.vars.ircal_rxamppll.value = 4
        model.vars.ircal_rxamppa.value = 16


    def calc_ircal_valid_methods(self, model):
        lodiv = model.vars.lodiv_actual.value
        subgig_band = model.vars.subgig_band.value
        part_family = model.part_family.lower()

        if part_family in ["dumbo","unit_test_part"]:
            if lodiv <= 3:
                model.vars.ircal_manufconfigvalid.value = True
            else:
                model.vars.ircal_manufconfigvalid.value = False

        else:  # part_family == "jumbo"
            if not subgig_band:
                model.vars.ircal_manufconfigvalid.value = True
            else:
                model.vars.ircal_manufconfigvalid.value = False

        if not subgig_band:
            model.vars.ircal_pllconfigvalid.value = False
        else:
            model.vars.ircal_pllconfigvalid.value = True

        if not subgig_band:
            model.vars.ircal_paconfigvalid.value = False
        else:
            model.vars.ircal_paconfigvalid.value = True


    def calc_ircal_determine_best_config(self, model):

        freq = model.vars.base_frequency_actual.value
        subgig_band = model.vars.subgig_band.value

        model.vars.ircal_rxtx_path_common.value = model.vars.ircal_rxtx_path_common.var_enum.SHARED_RX_TX_PATH

        ircal_rxtx_path_common = model.vars.ircal_rxtx_path_common.value

        model.vars.ircal_power_level.value = 0

        ircal_power_level = model.vars.ircal_power_level.value

        if not subgig_band:
            bestConfig = model.vars.ircal_bestconfig.var_enum.MANUFACTURING
        else:
            if (freq > 462000000) and ( ircal_rxtx_path_common.value == model.vars.ircal_rxtx_path_common.var_enum.SHARED_RX_TX_PATH):
                bestConfig = model.vars.ircal_bestconfig.var_enum.PA
            else:
                bestConfig = model.vars.ircal_bestconfig.var_enum.PLL

        model.vars.ircal_bestconfig.value = bestConfig


    def calc_ircal_auxpll_dividers(self, model):
        auxdividers = {2419000000 : ( 1, 63),  # For Lodiv = 1 Not currently used by the IRCal algorithm.
                        909000000 : ( 3, 71),  # For Lodiv = 3
                        883000000 : ( 3, 69),  # For Lodiv = 3 If the intent was to be near the 868 band, why wasn't (3,68) used to get 870?
                        643000000 : ( 4, 67),  # For Lodiv = 4 Untested
                        486000000 : ( 6, 76),  # For Lodiv = 5
                        435000000 : ( 6, 68),  # For Lodiv = 6
                        365000000 : ( 6, 58),  # For Lodiv = 7 # Added to support the 390MHz band.  Untested.
                        314000000 : ( 6, 49),  # For Lodiv = 8
                        288000000 : ( 8, 60),  # For Lodiv = 9 Untested
                        256000000 : ( 9, 60),  # For Lodiv =10 Untested
                        214000000 : (12, 67),  # For Lodiv =12 Untested
                        186000000 : (12, 58),  # For Lodiv =14 Untested
                        170000000 : (12, 53),  # For Lodiv =15
                        160000000 : (12, 50),  # For Lodiv =16 Untested
                        143000000 : (18, 67),  # For Lodiv =18 Untested
                        128000000 : (18, 60),  # For Lodiv =20 Untested
                        124000000 : (18, 58),  # For Lodiv =21 Untested
                        107000000 : (18, 50),  # For Lodiv =24 Untested
                        102000000 : (18, 48),  # For Lodiv =25 Untested
                       }

        rfFreq = model.vars.base_frequency_actual.value
        rfFreqMin = model.vars.tuning_limit_min.value
        rfFreqMax = model.vars.tuning_limit_max.value

        smallestDifference = sys.maxint # large number
        closestBandFreq = None

        for bandFreq in auxdividers.keys():
            if (bandFreq < rfFreqMax) and (bandFreq > rfFreqMin):
                difference = abs(rfFreq - bandFreq)
                if difference < smallestDifference:
                    smallestDifference = difference
                    closestBandFreq = bandFreq

        if closestBandFreq is not None:
            (auxLODiv, auxNDiv) = auxdividers[closestBandFreq]
            model.vars.ircal_auxndiv.value = auxNDiv
            model.vars.ircal_auxlodiv.value = auxLODiv
        else:
            if rfFreq > 100000000:
                raise CalculationException("Unexpected frequency band found.  Unable to calculate IR calibration values.")




