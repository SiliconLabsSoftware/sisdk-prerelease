"""This defines the CRC calculations and variables

Calculator functions are pulled by using their names.
Calculator functions must start with "calc_", if they are to be consumed by the framework.
    Or they should be returned by overriding the function:
        def getCalculationList(self):
"""

import inspect
from enum import Enum
from pyradioconfig.calculator_model_framework.interfaces.icalculator import ICalculator
from pycalcmodel.core.variable import ModelVariableFormat, CreateModelVariableEnum, ModelVariableEmptyValue, ModelVariableInvalidValueType
from pyradioconfig.parts.common.calculators.calc_crc import CALC_CRC as CALC_CRC_common

from py_2_and_3_compatibility import *

class CALC_CRC(CALC_CRC_common):
    """
    Init internal variables
    """



    def __init__(self):
        self._major = 1
        self._minor = 0
        self._patch = 0

    def buildVariables(self, model):
        """Populates a list of needed variables for this calculator

        Args:
            model (ModelRoot) : Builds the variables specific to this calculator
        """

        """
        """
        var = self._addModelVariable(model, 'crc_poly', Enum, ModelVariableFormat.DECIMAL,
                                     'List of supported CRC Polynomials')
        member_data = [
            ['NONE', 0, 'No CRC'],
            ['CRC_8', 1, 'X8+X2+X+1'],
            ['CRC_16', 2, 'X16+X15+X2+1'],
            ['CCITT_16', 3, 'X16+X12+X5+1'],
            ['DNP_16', 4, 'X16+X13+X12+X11+X10+X8+X6+X5+X2+1'],
            ['BLE_24', 5, 'X24+X10+X9+X6+X4+X3+X+1'],
            ['CRC_32Q', 6, 'X32+X31+X24+X22+X16+X14+X8+X7+X5+X3+X+1'],
            ['ANSIX366_1979', 7, 'X32+X26+X23+X22+X16+X12+X11+X10+X8+X7+X5+X4+X2+X+1'],
            ['ZWAVE', 8, 'X8+1'],
            ['BCH15_11', 9, 'X4+X+1'],
        ]
        var.var_enum = CreateModelVariableEnum(
            'CrcPolyEnum',
            'List of supported CRC Polynomials',
            member_data)

        self._addModelVariable(model, 'crc_seed', long, ModelVariableFormat.HEX, 'CRC Initialization Value')

        var = self._addModelVariable(model, 'crc_byte_endian', Enum, ModelVariableFormat.DECIMAL, 'CRC Byte Endianness')
        member_data = [
            ['LSB_FIRST', 0, 'Least significant byte of the CRC is transmitted first.'],
            ['MSB_FIRST', 1, 'Most significant byte of the CRC is transmitted first.'],
        ]
        var.var_enum = CreateModelVariableEnum(
            'CrcByteEndian',
            'Define how the CRC bytes are transmitted over the air',
            member_data)

        var = self._addModelVariable(model, 'crc_bit_endian', Enum, ModelVariableFormat.DECIMAL, 'CRC Bit Endianness')
        member_data = [
            ['LSB_FIRST', 0, 'Over the air CRC bit order is sent least significant bit first.'],
            ['MSB_FIRST', 1, 'Over the air CRC bit order is sent most significant bit first'],
        ]
        var.var_enum = CreateModelVariableEnum(
            'CrcBitEndian',
            'Define how the CRC bits are transmitted over the air',
            member_data)

        self._addModelVariable(model, 'crc_pad_input', bool, ModelVariableFormat.ASCII,
                               'Set to true to enable zero padding of the CRC input data.')

        var = self._addModelVariable(model, 'crc_input_order', Enum, ModelVariableFormat.DECIMAL,
                                     'Define the order data bits are fed into the CRC generator')
        member_data = [
            ['LSB_FIRST', 0, 'The least significant data bit is first input to the CRC generator'],
            ['MSB_FIRST', 1, 'The most significant data bit is first input to the CRC generator'],
        ]
        var.var_enum = CreateModelVariableEnum(
            'CrcInputOrderEnum',
            'Define the order data bits are fed into the CRC generator',
            member_data)

        self._addModelVariable(model, 'crc_invert', bool, ModelVariableFormat.ASCII,
                               'Set to true if the CRC result is inverted')



        """
        """
        self._build_crc_regs(model)

        self._addModelVariable(model, 'crc_polynomial', long, ModelVariableFormat.HEX, 'CRC polynomial')
        self._addModelVariable(model, 'crc_size', int, ModelVariableFormat.HEX, 'Size of CRC in bytes')

    def _build_crc_regs(self, model):
        self._addModelRegister(model, 'RFCRC.CTRL.PADCRCINPUT', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'RFCRC.CTRL.BITSPERWORD', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'RFCRC.CTRL.BITREVERSE', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'RFCRC.CTRL.BYTEREVERSE', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'RFCRC.CTRL.INPUTBITORDER', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'RFCRC.CTRL.CRCWIDTH', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'RFCRC.CTRL.OUTPUTINV', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'RFCRC.INIT.INIT', long, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'RFCRC.POLY.POLY', long, ModelVariableFormat.HEX)

    def _calc_init(self, model):
        """_calc_init

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        self._reg_write(model.vars.RFCRC_CTRL_PADCRCINPUT, 0)
        self._reg_write(model.vars.RFCRC_CTRL_BITREVERSE, 0)
        self._reg_write(model.vars.RFCRC_CTRL_BYTEREVERSE, 0)
        self._reg_write(model.vars.RFCRC_CTRL_INPUTBITORDER, 0)
        self._reg_write(model.vars.RFCRC_CTRL_CRCWIDTH, 0)
        self._reg_write(model.vars.RFCRC_CTRL_OUTPUTINV, 0)
        self._reg_write(model.vars.RFCRC_INIT_INIT, long(0))
        self._reg_write(model.vars.RFCRC_POLY_POLY, long(0))

        self._reg_write(model.vars.RFCRC_CTRL_BITSPERWORD, 7)



    def calc_crc_settings(self, model):
        """calc_crc_settings

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        CRC_POLY_LOOKUP = {
            model.vars.crc_poly.var_enum.NONE.value: (long(0), 0),
            model.vars.crc_poly.var_enum.CRC_8.value: (long(0x107), 1),
            model.vars.crc_poly.var_enum.CRC_16.value: (long(0x18005), 2),
            model.vars.crc_poly.var_enum.CCITT_16.value: (long(0x11021), 2),
            model.vars.crc_poly.var_enum.DNP_16.value: (long(0x13d65), 2),
            model.vars.crc_poly.var_enum.BLE_24.value: (long(0x100065b), 3),
            model.vars.crc_poly.var_enum.CRC_32Q.value: (long(0x1814141ab), 4),
            model.vars.crc_poly.var_enum.ANSIX366_1979.value: (long(0x104c11db7), 4),
            model.vars.crc_poly.var_enum.ZWAVE.value: (long(0x101), 1),
            model.vars.crc_poly.var_enum.BCH15_11.value: (long(0x13), 1),
        }

        self._calc_init(model)

        model.vars.crc_polynomial.value = CRC_POLY_LOOKUP[model.vars.crc_poly.value.value][0]
        model.vars.crc_size.value = CRC_POLY_LOOKUP[model.vars.crc_poly.value.value][1]

        if model.vars.crc_poly.value.value != model.vars.crc_poly.var_enum.NONE.value:
            poly_reg, seed_reg = self._calc_crc_poly_reg(model.vars.crc_polynomial.value, model.vars.crc_seed.value)
            self._reg_write(model.vars.RFCRC_POLY_POLY, poly_reg)
            self._reg_write(model.vars.RFCRC_INIT_INIT, seed_reg)

            self._reg_write(model.vars.RFCRC_CTRL_CRCWIDTH, model.vars.crc_size.value - 1)

            if model.vars.crc_byte_endian.value == model.vars.crc_byte_endian.var_enum.LSB_FIRST:
                self._reg_write(model.vars.RFCRC_CTRL_BYTEREVERSE, 1)
            if model.vars.frame_bitendian.value.value == model.vars.crc_bit_endian.value.value:
                self._reg_write(model.vars.RFCRC_CTRL_BITREVERSE, 1)

            if model.vars.crc_input_order.value == model.vars.crc_input_order.var_enum.MSB_FIRST:
                self._reg_write(model.vars.RFCRC_CTRL_INPUTBITORDER, 1)

            if model.vars.crc_pad_input.value is True:
                self._reg_write(model.vars.RFCRC_CTRL_PADCRCINPUT, 1)

            if model.vars.crc_invert.value is True:
                self._reg_write(model.vars.RFCRC_CTRL_OUTPUTINV, 1)

        return
