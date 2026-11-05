"""
This defines generic frame/packet configurations and calculation
This file also houses calculations for fields that are affected by multiiple blocks:
  calc_blockwhitemode(): calc_white, calc_fec

"""
"""
Calculator functions are pulled by using their names.
Calculator functions must start with "calc_", if they are to be consumed by the framework.
    Or they should be returned by overriding the function:
        def getCalculationList(self):
"""

import inspect
from enum import Enum
from pyradioconfig.calculator_model_framework.interfaces.icalculator import ICalculator
from pycalcmodel.core.variable import ModelVariableFormat, CreateModelVariableEnum
from pyradioconfig.calculator_model_framework.Utils.CustomExceptions import CalculationException


class CALC_Frame(ICalculator):

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
        var = self._addModelVariable(model, 'frame_bitendian', Enum, ModelVariableFormat.DECIMAL, 'Define how the payload bits are transmitted over the air')
        member_data = [
            ['LSB_FIRST' , 0, 'Least significant bit is transmitted first over the air'],
            ['MSB_FIRST',  1, 'Most significant bit is transmitted first over the air'],
        ]
        var.var_enum = CreateModelVariableEnum(
            'bitEndian',
            'Define how the payload bits are transmitted over the air',
            member_data)

        self._addModelVariable(model, 'firstframe_bitsperword', int, ModelVariableFormat.DECIMAL, units='bits', desc='On reception, create the first received bytes from less than 8 bits. This can be used to "bitshift" the frame. Upper bits are padded with 0 in the downloaded frame.')


        var = self._addModelVariable(model, 'frame_length_type', Enum, ModelVariableFormat.DECIMAL, 'Possible Length Configurations')
        member_data = [
            ['FIXED_LENGTH' , 0, 'The frame length is fixed and never changes'],
            ['VARIABLE_LENGTH',  1, 'The frame length is determined by an explicit length field within the packet. Requires header to be enabled.'],
            ['FRAME_TYPE',  2, 'The packet length is determined from an encoded set of bit that implicitly determines the length'],
        ]
        var.var_enum = CreateModelVariableEnum(
            'FrameLengthEnum',
            'List of supported frame length configurations',
            member_data)

        self._addModelVariable(model, 'payload_white_en', bool, ModelVariableFormat.ASCII, 'Set to true to whiten the payload')
        self._addModelVariable(model, 'payload_crc_en', bool, ModelVariableFormat.ASCII, 'Set to true to check/transmit crc after the payload')
        self._addModelVariable(model, 'accept_crc_errors', bool, ModelVariableFormat.ASCII, 'Set to true if you want to accept invalid crcs')

        self._addModelVariable(model, 'payload_addtrailtxdata_en', bool, ModelVariableFormat.ASCII, 'Set to true to add Trail TX data at the end of the frame')

        self._addModelVariable(model, 'payload_excludesubframewcnt_en', bool, ModelVariableFormat.ASCII, 'Set to true to exclude words in the subframe from the Word Counter (WCNT), useful in Dynamic Frame Length (DFL) mode')

        self._addModelVariable(model, 'header_en', bool, ModelVariableFormat.ASCII, 'Set to true to enable a distinct header from the payload.')
        self._addModelVariable(model, 'header_size', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Define the number of bytes that make up the header. Include the variable length byte(s).')
        self._addModelVariable(model, 'header_calc_crc', bool, ModelVariableFormat.ASCII, 'Set to true to include the header bytes in the payload CRC.')
        self._addModelVariable(model, 'header_include_crc', bool, ModelVariableFormat.ASCII, 'Set to true to check/transmit crc specifically for the header')
        self._addModelVariable(model, 'header_white_en', bool, ModelVariableFormat.ASCII, 'Set to true to enable whitening over the header')

        self._addModelVariable(model, 'header_addtrailtxdata_en', bool, ModelVariableFormat.ASCII, 'Set to true to add Trail TX data at the end of the frame')

        self._addModelVariable(model, 'header_excludesubframewcnt_en', bool, ModelVariableFormat.ASCII, 'Set to true to exclude words in the subframe from the Word Counter (WCNT), useful in Dynamic Frame Length (DFL) mode')

        self._addModelVariable(model, 'fixed_length_size', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Define the number of bytes in the payload. This does not include the length of the header if used. Header + Payload must be less than 4096 bytes.')

        self._addModelVariable(model, 'var_length_numbits', int, ModelVariableFormat.DECIMAL, units='bits', desc='Define the size of the variable length field in bits.')
        var = self._addModelVariable(model, 'var_length_byteendian', Enum, ModelVariableFormat.DECIMAL, 'Define the byte endianness of the variable length field')
        member_data = [
            ['LSB_FIRST' , 0, 'The least significant byte of the variable length field is transmitted over the air first.'],
            ['MSB_FIRST' , 1, 'The most significant byte of the variable length field is transmitted over the air first.'],
        ]
        var.var_enum = CreateModelVariableEnum(
            'VarLengthByteEndian',
            'Define how the variable length byte(s) are transmitted over the air',
            member_data)
        var = self._addModelVariable(model, 'var_length_bitendian', Enum, ModelVariableFormat.DECIMAL, 'Define the bit endianness of the variable length field')
        member_data = [
            ['LSB_FIRST' , 0, 'The variable length field is transmitted least signficant bit first.'],
            ['MSB_FIRST' , 1, 'The variable length field is transmitted most significant bit first.'],
        ]
        var.var_enum = CreateModelVariableEnum(
            'VarLengthBitEndian',
            'Define how the variable length bits are transmitted over the air',
            member_data)
        self._addModelVariable(model, 'var_length_shift', int, ModelVariableFormat.DECIMAL, 'Define the location of the least significant bit of the variable length field.')
        self._addModelVariable(model, 'var_length_minlength', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Define the minimum value of the variable length field.')
        self._addModelVariable(model, 'var_length_maxlength', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Define the maximum value of the variable length field. Cannot exceed the variable length size.')
        self._addModelVariable(model, 'var_length_includecrc', bool, ModelVariableFormat.ASCII, 'Set to true if the crc bytes are included in the variable length')
        self._addModelVariable(model, 'var_length_adjust', int, ModelVariableFormat.DECIMAL, 'Value to add to the variable length extracted from the packet when calculating the total payload length to receive.  A positive number here indicates the payload will be larger than the length value extracted from the variable length bits.')

        self._addModelVariable(model, 'frame_type_loc', int, ModelVariableFormat.DECIMAL, 'Define the zero-based start location in the frame that holds the frame type encoding.')
        self._addModelVariable(model, 'frame_type_mask', int, ModelVariableFormat.HEX, 'Define the bitmask to extract the frame type in the byte.')
        self._addModelVariable(model, 'frame_type_bits', int, ModelVariableFormat.DECIMAL, desc='Define the number of bits of the frame type field.', units='bits')
        self._addModelVariable(model, 'frame_type_lsbit', int, ModelVariableFormat.DECIMAL, "Define the bit location of the frame type's least significant bit.")
        self._addModelVariable(model, 'frame_type_lengths', int, ModelVariableFormat.DECIMAL, desc='Define the frame length of each frame type.', is_array=True, units='bytes')
        self._addModelVariable(model, 'frame_type_valid', bool, ModelVariableFormat.ASCII, desc='Define the valid frame types.', is_array=True)
        self._addModelVariable(model, 'frame_type_filter', bool, ModelVariableFormat.ASCII, desc='Define the frame types that should have address filtering applied.', is_array=True)

        self._addModelVariable(model, 'frame_type_0_length', int, ModelVariableFormat.DECIMAL, desc='Define the frame length of each frame type.', units='bytes')
        self._addModelVariable(model, 'frame_type_1_length', int, ModelVariableFormat.DECIMAL, desc='Define the frame length of each frame type.', units='bytes')
        self._addModelVariable(model, 'frame_type_2_length', int, ModelVariableFormat.DECIMAL, desc='Define the frame length of each frame type.', units='bytes')
        self._addModelVariable(model, 'frame_type_3_length', int, ModelVariableFormat.DECIMAL, desc='Define the frame length of each frame type.', units='bytes')
        self._addModelVariable(model, 'frame_type_4_length', int, ModelVariableFormat.DECIMAL, desc='Define the frame length of each frame type.', units='bytes')
        self._addModelVariable(model, 'frame_type_5_length', int, ModelVariableFormat.DECIMAL, desc='Define the frame length of each frame type.', units='bytes')
        self._addModelVariable(model, 'frame_type_6_length', int, ModelVariableFormat.DECIMAL, desc='Define the frame length of each frame type.', units='bytes')
        self._addModelVariable(model, 'frame_type_7_length', int, ModelVariableFormat.DECIMAL, desc='Define the frame length of each frame type.', units='bytes')

        self._addModelVariable(model, 'frame_type_0_valid', bool, ModelVariableFormat.ASCII, desc='Define the valid frame types.')
        self._addModelVariable(model, 'frame_type_1_valid', bool, ModelVariableFormat.ASCII, desc='Define the valid frame types.')
        self._addModelVariable(model, 'frame_type_2_valid', bool, ModelVariableFormat.ASCII, desc='Define the valid frame types.')
        self._addModelVariable(model, 'frame_type_3_valid', bool, ModelVariableFormat.ASCII, desc='Define the valid frame types.')
        self._addModelVariable(model, 'frame_type_4_valid', bool, ModelVariableFormat.ASCII, desc='Define the valid frame types.')
        self._addModelVariable(model, 'frame_type_5_valid', bool, ModelVariableFormat.ASCII, desc='Define the valid frame types.')
        self._addModelVariable(model, 'frame_type_6_valid', bool, ModelVariableFormat.ASCII, desc='Define the valid frame types.')
        self._addModelVariable(model, 'frame_type_7_valid', bool, ModelVariableFormat.ASCII, desc='Define the valid frame types.')

        self._addModelVariable(model, 'frame_type_0_filter', bool, ModelVariableFormat.ASCII, desc='Define the frame types that should have address filtering applied.')
        self._addModelVariable(model, 'frame_type_1_filter', bool, ModelVariableFormat.ASCII, desc='Define the frame types that should have address filtering applied.')
        self._addModelVariable(model, 'frame_type_2_filter', bool, ModelVariableFormat.ASCII, desc='Define the frame types that should have address filtering applied.')
        self._addModelVariable(model, 'frame_type_3_filter', bool, ModelVariableFormat.ASCII, desc='Define the frame types that should have address filtering applied.')
        self._addModelVariable(model, 'frame_type_4_filter', bool, ModelVariableFormat.ASCII, desc='Define the frame types that should have address filtering applied.')
        self._addModelVariable(model, 'frame_type_5_filter', bool, ModelVariableFormat.ASCII, desc='Define the frame types that should have address filtering applied.')
        self._addModelVariable(model, 'frame_type_6_filter', bool, ModelVariableFormat.ASCII, desc='Define the frame types that should have address filtering applied.')
        self._addModelVariable(model, 'frame_type_7_filter', bool, ModelVariableFormat.ASCII, desc='Define the frame types that should have address filtering applied.')

        """
        Internal variables
        """
        self._addModelVariable(model, 'var_length_loc', int, ModelVariableFormat.DECIMAL, 'Define the zero-based start location in the header that holds the first byte of the variable length field.')
        self._addModelVariable(model, 'var_length_numbytes', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Define the size of the variable length field in bytes.')

        self._addModelVariable(model, 'header_size_internal', int, ModelVariableFormat.DECIMAL, units='bytes', desc='Internal representation of header size. 0 for no header.')

        self._addModelVariable(model, 'fcdx_words_bitwidth', int, ModelVariableFormat.DECIMAL, units='bytes', desc='FCD_WORDS bitwidth.')

        """
        """
        self._build_frame_regs(model)

    def _build_frame_regs(self, model):
        self._addModelRegister(model, 'FRC.CTRL.BITSPERWORD', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.CTRL.RXFCDMODE', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.CTRL.TXFCDMODE', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.CTRL.BITORDER', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.CTRL.UARTMODE', int, ModelVariableFormat.HEX)

        self._addModelRegister(model, 'FRC.WCNTCMP0.FRAMELENGTH', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.WCNTCMP1.LENGTHFIELDLOC', int, ModelVariableFormat.HEX)

        self._addModelRegister(model, 'FRC.DFLCTRL.DFLINCLUDECRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.DFLCTRL.MINLENGTH', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.DFLCTRL.DFLBITS', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.DFLCTRL.DFLOFFSET', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.DFLCTRL.DFLSHIFT', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.DFLCTRL.DFLBITORDER', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.DFLCTRL.DFLMODE', int, ModelVariableFormat.HEX)

        self._addModelRegister(model, 'FRC.MAXLENGTH.MAXLENGTH', int, ModelVariableFormat.HEX)

        self._addModelRegister(model, 'FRC.FCD0.SKIPWHITE', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD0.SKIPCRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD0.CALCCRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD0.INCLUDECRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD0.BUFFER', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD0.WORDS', int, ModelVariableFormat.HEX)

        self._addModelRegister(model, 'FRC.FCD1.SKIPWHITE', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD1.SKIPCRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD1.CALCCRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD1.INCLUDECRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD1.BUFFER', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD1.WORDS', int, ModelVariableFormat.HEX)

        self._addModelRegister(model, 'FRC.FCD2.SKIPWHITE', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD2.SKIPCRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD2.CALCCRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD2.INCLUDECRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD2.BUFFER', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD2.WORDS', int, ModelVariableFormat.HEX)

        self._addModelRegister(model, 'FRC.FCD3.SKIPWHITE', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD3.SKIPCRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD3.CALCCRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD3.INCLUDECRC', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD3.BUFFER', int, ModelVariableFormat.HEX)
        self._addModelRegister(model, 'FRC.FCD3.WORDS', int, ModelVariableFormat.HEX)

    def _calc_frame_length_defaults(self, model):
        """_calc_frame_length_defaults

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        self._reg_write(model.vars.FRC_WCNTCMP0_FRAMELENGTH, 0)
        self._reg_write(model.vars.FRC_WCNTCMP1_LENGTHFIELDLOC, 0)

        self._reg_write(model.vars.FRC_DFLCTRL_DFLINCLUDECRC, 0)
        self._reg_write(model.vars.FRC_DFLCTRL_MINLENGTH, 0)
        self._reg_write(model.vars.FRC_DFLCTRL_DFLBITS, 0)
        self._reg_write(model.vars.FRC_DFLCTRL_DFLOFFSET, 0)
        self._reg_write(model.vars.FRC_DFLCTRL_DFLSHIFT, 0)
        self._reg_write(model.vars.FRC_DFLCTRL_DFLBITORDER, 0)
        if (model.vars.ber_force_infinite_length.value == True):
            self._reg_write(model.vars.FRC_DFLCTRL_DFLMODE, 5)
        else:
            self._reg_write(model.vars.FRC_DFLCTRL_DFLMODE, 0)



        self._reg_write(model.vars.FRC_MAXLENGTH_MAXLENGTH, 0)

    def _configure_fcd(self, model, fcdindex, excludesubframewcnt = 0, addtrailtxdata = 0, skipwhite = 0,
                                              skipcrc = 0, calccrc = 0, includecrc = 0, buf = 0, words = 0):

        """_configure_fcd

        Args:
            model (ModelRoot) : Data model to read and write variables from
            fcdindex (unknown) : unknown
            excludesubframewcnt (unknown) : unknown
            addtrailtxdata (unknown) : unknown
            skipwhite (unknown) : unknown
            skipcrc (unknown) : unknown
            calccrc (unknown) : unknown
            includecrc (unknown) : unknown
            buf (unknown) : unknown
            words (unknown) : unknown
        """

        part_family = model.part_family.lower()

        if part_family == "nerio":
            self._reg_write(eval("model.vars.FRC_FCD{}_EXCLUDESUBFRAMEWCNT".format(fcdindex)), excludesubframewcnt)

        if part_family not in ['dumbo','unit_test_part']:
            self._reg_write(eval("model.vars.FRC_FCD{}_ADDTRAILTXDATA".format(fcdindex)), addtrailtxdata)

        if model.vars.ber_force_whitening.value == True:
            self._reg_write(eval("model.vars.FRC_FCD{}_SKIPWHITE".format(fcdindex)), 0)
        else:
            self._reg_write(eval("model.vars.FRC_FCD{}_SKIPWHITE".format(fcdindex)), skipwhite)

        self._reg_write(eval("model.vars.FRC_FCD{}_SKIPCRC".format(fcdindex)), skipcrc)
        self._reg_write(eval("model.vars.FRC_FCD{}_CALCCRC".format(fcdindex)), calccrc)
        self._reg_write(eval("model.vars.FRC_FCD{}_INCLUDECRC".format(fcdindex)), includecrc)
        self._reg_write(eval("model.vars.FRC_FCD{}_BUFFER".format(fcdindex)), buf)
        self._reg_write(eval("model.vars.FRC_FCD{}_WORDS".format(fcdindex)), words)
        return

    def _configure_header(self, model):
        """_configure_header

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        fcdDict = {
            "excludesubframewcnt": int(model.vars.header_excludesubframewcnt_en.value == True),
            "addtrailtxdata": int(model.vars.header_addtrailtxdata_en.value == True),
            "skipwhite": int(model.vars.header_white_en.value == False),
            "skipcrc": 0,
            "calccrc": int(model.vars.header_calc_crc.value == True),
            "includecrc": int(model.vars.header_include_crc.value == True),
            "words": model.vars.header_size_internal.value - 1,
        }
        self._configure_fcd(model, fcdindex="0", buf=0, **fcdDict)
        self._configure_fcd(model, fcdindex="2", buf=1, **fcdDict)
        return

    def _configure_payload_with_header(self, model):
        """_configure_payload_with_header

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        fcdx_words_bitwidth = model.vars.fcdx_words_bitwidth.value

        fcdDict = {
            "excludesubframewcnt": int(model.vars.payload_excludesubframewcnt_en.value == True),
            "addtrailtxdata": int(model.vars.payload_addtrailtxdata_en.value == True),
            "skipwhite": int(model.vars.payload_white_en.value == False),
            "skipcrc": 0,
            "calccrc": int(model.vars.payload_crc_en.value == True),
            "includecrc": int(model.vars.payload_crc_en.value == True),
            "words": 2 ** fcdx_words_bitwidth - 1,
        }
        self._configure_fcd(model, fcdindex="1", buf=0, **fcdDict)
        self._configure_fcd(model, fcdindex="3", buf=1, **fcdDict)
        return

    def _fixed_length_with_header(self, model):
        """_fixed_length_with_header

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        self._reg_write(model.vars.FRC_WCNTCMP0_FRAMELENGTH, model.vars.fixed_length_size.value + model.vars.header_size_internal.value - 1)

        self._configure_header(model)

        self._configure_payload_with_header(model)

        self._reg_write(model.vars.FRC_CTRL_TXFCDMODE, 2)
        self._reg_write(model.vars.FRC_CTRL_RXFCDMODE, 2)
        return

    def _fixed_length_no_header(self, model):
        """_fixed_length_no_header

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        fcdx_words_bitwidth = model.vars.fcdx_words_bitwidth.value

        self._reg_write(model.vars.FRC_WCNTCMP0_FRAMELENGTH, model.vars.fixed_length_size.value - 1)
        fcdDict = {
            "excludesubframewcnt": int(model.vars.payload_excludesubframewcnt_en.value == True),
            "addtrailtxdata": int(model.vars.payload_addtrailtxdata_en.value == True),
            "skipwhite": int(model.vars.payload_white_en.value == False),
            "skipcrc": 0,
            "calccrc": int(model.vars.payload_crc_en.value == True),
            "includecrc": int(model.vars.payload_crc_en.value == True),
            "words": 2 ** fcdx_words_bitwidth - 1,
        }
        self._configure_fcd(model, fcdindex="0", buf=0, **fcdDict)
        self._configure_fcd(model, fcdindex="2", buf=1, **fcdDict)

        self._configure_fcd(model, 1)   # Turn off this fcd
        self._configure_fcd(model, 3)   # Turn off this fcd

        self._reg_write(model.vars.FRC_CTRL_TXFCDMODE, 0)
        self._reg_write(model.vars.FRC_CTRL_RXFCDMODE, 0)

        return

    def _configure_fcd_for_frame_type(self, model):

        fcdx_words_bitwidth = model.vars.fcdx_words_bitwidth.value

        fcdDict = {
            "excludesubframewcnt": int(model.vars.payload_excludesubframewcnt_en.value == True),
            "addtrailtxdata": int(model.vars.payload_addtrailtxdata_en.value == True),
            "skipwhite": int(model.vars.payload_white_en.value == False),
            "skipcrc": 0,
            "calccrc": int(model.vars.payload_crc_en.value == True),
            "includecrc": int(model.vars.payload_crc_en.value == True),
            "words":  2 ** fcdx_words_bitwidth - 1,
        }
        self._configure_fcd(model, fcdindex="0", buf=0, **fcdDict)
        self._reg_write(model.vars.FRC_CTRL_TXFCDMODE, 0)
        self._configure_fcd(model, fcdindex="2", buf=1, **fcdDict)
        self._reg_write(model.vars.FRC_CTRL_RXFCDMODE, 0)

        self._configure_fcd(model, 1)   # Turn off this fcd
        self._configure_fcd(model, 3)   # Turn off this fcd




    def _configure_variable_length(self, model):
        """_configure_variable_length

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        self._reg_write(model.vars.FRC_WCNTCMP0_FRAMELENGTH, 0)

        self._reg_write(model.vars.FRC_WCNTCMP1_LENGTHFIELDLOC, model.vars.var_length_loc.value)
        self._reg_write(model.vars.FRC_MAXLENGTH_MAXLENGTH, model.vars.var_length_maxlength.value+model.vars.header_size_internal.value-1)

        self._reg_write(model.vars.FRC_DFLCTRL_DFLINCLUDECRC, int(model.vars.var_length_includecrc.value == True))

        if model.vars.var_length_numbits.value == 0 and model.vars.frame_length_type.value == model.vars.frame_length_type.var_enum.VARIABLE_LENGTH:
            raise CalculationException("ERROR: var_length_numbits == 0 and frame_length_type == VARIABLE_LENGTH is not allowed.")

        self._reg_write(model.vars.FRC_DFLCTRL_MINLENGTH, model.vars.var_length_minlength.value+model.vars.header_size_internal.value-1)
        self._reg_write(model.vars.FRC_DFLCTRL_DFLBITS, model.vars.var_length_numbits.value)
        self._reg_write(model.vars.FRC_DFLCTRL_DFLOFFSET, model.vars.header_size_internal.value + model.vars.var_length_adjust.value-1, allow_neg=True)
        self._reg_write(model.vars.FRC_DFLCTRL_DFLSHIFT, model.vars.var_length_shift.value)
        if (model.vars.var_length_bitendian.value.value != model.vars.frame_bitendian.value.value):
            self._reg_write(model.vars.FRC_DFLCTRL_DFLBITORDER, 1)
        else:
            self._reg_write(model.vars.FRC_DFLCTRL_DFLBITORDER, 0)

        if (model.vars.ber_force_infinite_length.value == True):
            self._reg_write(model.vars.FRC_DFLCTRL_DFLMODE, 5)
        elif (model.vars.var_length_numbytes.value == 1):
            self._reg_write(model.vars.FRC_DFLCTRL_DFLMODE, 1)
        else: #Two bytes
            if (model.vars.var_length_byteendian.value == model.vars.var_length_byteendian.var_enum.LSB_FIRST):
                self._reg_write(model.vars.FRC_DFLCTRL_DFLMODE, 3)
            else:
                self._reg_write(model.vars.FRC_DFLCTRL_DFLMODE, 4)
        return

    def _configure_frame_type(self, model):
        """_configure_frame_type

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        BIT_MASK = {0: 0x00, 1: 0x01, 2: 0x03, 3: 0x07}

        num_entries = (2**model.vars.frame_type_bits.value)
        model.vars.frame_type_lengths.value = []
        model.vars.frame_type_valid.value = []
        model.vars.frame_type_filter.value = []
        for i in range(num_entries):
          model.vars.frame_type_lengths.value.append(eval("model.vars.frame_type_{}_length.value".format(i)))
          model.vars.frame_type_valid.value.append(eval("model.vars.frame_type_{}_valid.value".format(i)))
          model.vars.frame_type_filter.value.append(eval("model.vars.frame_type_{}_filter.value".format(i)))

        min_size = 0xFF
        for i in range(len(model.vars.frame_type_lengths.value)):
          if (model.vars.frame_type_valid.value[i] == True):
            if (model.vars.frame_type_lengths.value[i] < min_size):
              min_size = model.vars.frame_type_lengths.value[i]

        self._reg_write(model.vars.FRC_WCNTCMP0_FRAMELENGTH, min_size - 1)

        model.vars.frame_type_mask.value = BIT_MASK[model.vars.frame_type_bits.value] << model.vars.frame_type_lsbit.value

        return

    def calc_possible_future_inputs(self, model):
        """

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        model.vars.header_include_crc.value = False


        model.vars.header_addtrailtxdata_en.value = False
        model.vars.payload_addtrailtxdata_en.value = False

        model.vars.header_excludesubframewcnt_en.value = False
        model.vars.payload_excludesubframewcnt_en.value = False

        return

    def calc_frame(self, model):
        """
        Configure general frame configurations

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        model.vars.firstframe_bitsperword.value = 8

        bitsperword_reg = model.vars.firstframe_bitsperword.value - 1

        self._reg_write(model.vars.FRC_CTRL_BITSPERWORD, bitsperword_reg)

        if model.vars.ber_force_bitorder.value == True:
            self._reg_write(model.vars.FRC_CTRL_BITORDER, 1)
        else:
            self._reg_write(model.vars.FRC_CTRL_BITORDER, int(model.vars.frame_bitendian.value == model.vars.frame_bitendian.var_enum.MSB_FIRST))
        return

    def calc_frame_length(self, model):
        """calc_frame_length

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        if (model.vars.frame_length_type.value == model.vars.frame_length_type.var_enum.FIXED_LENGTH):
            if (model.vars.header_en.value == True):
                self._calc_frame_length_defaults(model)
                self._fixed_length_with_header(model)
            else:
                self._calc_frame_length_defaults(model)
                self._fixed_length_no_header(model)

        elif (model.vars.frame_length_type.value == model.vars.frame_length_type.var_enum.VARIABLE_LENGTH and model.vars.header_en.value == True):
            self._configure_header(model)

            self._configure_payload_with_header(model)

            self._configure_variable_length(model)

            self._reg_write(model.vars.FRC_CTRL_TXFCDMODE, 2)
            self._reg_write(model.vars.FRC_CTRL_RXFCDMODE, 2)
        elif (model.vars.frame_length_type.value == model.vars.frame_length_type.var_enum.FRAME_TYPE):
            self._calc_frame_length_defaults(model)
            self._configure_fcd_for_frame_type(model)

            self._configure_frame_type(model)
            pass
        return


    def calc_var_length_numbytes(self, model):
        """calc_var_length_numbytes

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        quotient, remainder = divmod((model.vars.var_length_numbits.value + model.vars.var_length_shift.value), 8)
        if (remainder > 0):
            model.vars.var_length_numbytes.value = quotient + 1
        else:
            model.vars.var_length_numbytes.value = quotient

    def calc_var_length_loc(self, model):
        """
        The variable length location must be the last 1 or 2 bytes of the header

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """


        model.vars.var_length_loc.value = model.vars.header_size_internal.value - model.vars.var_length_numbytes.value

        if model.vars.var_length_loc.value < 0:
            model.vars.var_length_loc.value = 0

    def calc_header_bytes(self, model):
        if model.vars.header_en.value == True:
            model.vars.header_size_internal.value = model.vars.header_size.value
        else:
            model.vars.header_size_internal.value = 0

    def calc_check_crc_poly(self, model):

        header_include_crc = model.vars.header_include_crc.value
        payload_crc_en = model.vars.payload_crc_en.value
        crc_poly = model.vars.crc_poly.value.value

        if header_include_crc == True or payload_crc_en == True:
            if crc_poly == model.vars.crc_poly.var_enum.NONE.value:
                raise CalculationException("ERROR: CRC enabled with crc polynomial set to NONE")

    def calc_fcdx_words_bitwidth(self, model):
        if model.vars.FRC_FCD0_WORDS.rm is None:
            val = 8
        else:
            val = model.vars.FRC_FCD0_WORDS.get_bit_width()
        model.vars.fcdx_words_bitwidth.value = val