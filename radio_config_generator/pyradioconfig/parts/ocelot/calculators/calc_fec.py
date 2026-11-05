from pyradioconfig.parts.lynx.calculators.calc_fec import CALC_FEC_Lynx
from pycalcmodel.core.variable import ModelVariableFormat, CreateModelVariableEnum
from enum import Enum

class CALC_FEC_Ocelot(CALC_FEC_Lynx):

    def __init__(self):
        super().__init__()

    def buildVariables(self, model):
        super().buildVariables(model)

        self._addModelVariable(model, 'mbus_postamble_length', int, ModelVariableFormat.DECIMAL, 'Mbus postamble legnth in sets of two alternating chips')

    def _build_fec_var(self, model):

        var = self._addModelVariable(model, 'fec_en', Enum, ModelVariableFormat.DECIMAL,
                                     'List of supported FEC Configurations')
        member_data = [
            ['NONE', 0, 'No FEC'],
            ['FEC_154G_NRNSC_INTERLEAVING',  1, '15.4G FEC settings with NRNSC and interleaving'],
            ['FEC_154G_RSC_INTERLEAVING', 2, '15.4G FEC settings with RSC and interleaving'],
            ['FEC_154G_RSC_NO_INTERLEAVING', 3, '15.4G FEC settings with RSC and no interleaving'],
            ['FEC_K7_INTERLEAVING', 4, 'FEC settings with K=7 with interleaving']
        ]
        var.var_enum = CreateModelVariableEnum(
            'FECEnum',
            'List of supported FEC Configurations',
            member_data)

    def calc_fec(self, model):

        model.vars.fec_en.value = model.vars.fec_en.var_enum.NONE  # Calculate a default value for Profiles where this is an advanced input

        self._calc_init(model)

        if model.vars.fec_en.value != model.vars.fec_en.var_enum.NONE:
            self._FEC_154G_Base(model)

            if model.vars.fec_en.value == model.vars.fec_en.var_enum.FEC_K7_INTERLEAVING:
                self._reg_write(model.vars.FRC_CONVGENERATOR_GENERATOR0, 0x6D)
                self._reg_write(model.vars.FRC_CONVGENERATOR_GENERATOR1, 0x4F)

    def calc_fec_enabled(self, model):
        model.vars.fec_enabled.value = int(model.vars.FRC_FECCTRL_CONVMODE.value != 0)

    def calc_postamble_regs(self, model):

        model.vars.mbus_postamble_length.value = 0
        mbus_symbol_encoding = model.vars.mbus_symbol_encoding.value
        profile = model.profile.name.lower()
        fec_enable = model.vars.fec_en.value
        postamble_length = model.vars.mbus_postamble_length.value

        if profile == 'mbus':
            if mbus_symbol_encoding == model.vars.mbus_symbol_encoding.var_enum.Manchester:
                trailtxdataforce = 1
                trailtxdata = 0xff
                postambleen = 0
                trailtxreplen = 0
                if postamble_length == 1:
                    trailtxdatacnt = 0
                elif postamble_length == 2:
                    trailtxdatacnt = 1
                elif postamble_length == 3:
                    trailtxdatacnt = 2
                elif postamble_length == 4:
                    trailtxdatacnt = 3
                else:
                    trailtxdatacnt = 0
            elif mbus_symbol_encoding == model.vars.mbus_symbol_encoding.var_enum.MBUS_3OF6:
                trailtxdataforce = 1
                trailtxdata = 0
                postambleen = 1
                trailtxreplen = 0
                if postamble_length == 1:
                    trailtxdatacnt = 1
                elif postamble_length == 2:
                    trailtxdatacnt = 3
                elif postamble_length == 3:
                    trailtxdatacnt = 5
                elif postamble_length == 4:
                    trailtxdatacnt = 7
                else:
                    trailtxdatacnt = 1
            else:
                trailtxdataforce = 0
                trailtxdata = 0
                trailtxdatacnt = 0
                postambleen = 0
                trailtxreplen = 0
        elif fec_enable != model.vars.fec_en.var_enum.NONE:
            trailtxdata = 0x0B
            trailtxdatacnt = 0
            postambleen = 0
            trailtxdataforce = 0
            trailtxreplen = 0
        else:
            trailtxdataforce = 0
            trailtxdata = 0
            trailtxdatacnt = 0
            postambleen = 0
            trailtxreplen = 0

        self._reg_write(model.vars.FRC_TRAILTXDATACTRL_TRAILTXDATA, trailtxdata)
        self._reg_write(model.vars.FRC_TRAILTXDATACTRL_TRAILTXDATAFORCE, trailtxdataforce)
        self._reg_write(model.vars.FRC_TRAILTXDATACTRL_POSTAMBLEEN, postambleen)
        self._reg_write(model.vars.FRC_TRAILTXDATACTRL_TRAILTXDATACNT, trailtxdatacnt)
        self._reg_write(model.vars.FRC_TRAILTXDATACTRL_TRAILTXREPLEN, trailtxreplen)

    def calc_feccrl_interleavemode_reg(self, model):

        fec_en = model.vars.fec_en.value
        fec_enabled = model.vars.fec_enabled.value

        if fec_enabled:
            if (fec_en == model.vars.fec_en.var_enum.FEC_154G_NRNSC_INTERLEAVING) or \
                    (fec_en == model.vars.fec_en.var_enum.FEC_154G_RSC_INTERLEAVING) or \
                    (fec_en == model.vars.fec_en.var_enum.FEC_K7_INTERLEAVING):
                interleavemode = 1
            else:
                interleavemode = 0
        else:
            interleavemode = 0

        self._reg_write(model.vars.FRC_FECCTRL_INTERLEAVEMODE, interleavemode)

    def calc_convdecodemode_reg(self, model):

        fec_en = model.vars.fec_en.value
        fec_enabled = model.vars.fec_enabled.value

        if fec_enabled or (fec_en != model.vars.fec_en.var_enum.NONE):
            convdecodemode = 1  # hard decision (default in most cases)
        else:
            convdecodemode = 0

        self._reg_write(model.vars.FRC_FECCTRL_CONVDECODEMODE, convdecodemode)