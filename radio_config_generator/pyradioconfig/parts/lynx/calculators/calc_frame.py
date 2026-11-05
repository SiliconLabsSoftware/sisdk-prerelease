from pyradioconfig.parts.panther.calculators.calc_frame import CALC_Frame_panther

class CALC_Frame_lynx(CALC_Frame_panther):
    def _combine_rx_fcds(self, model):
        """_combine_rx_fcds

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        header_no_trail = (model.vars.header_addtrailtxdata_en.value == False)
        header_no_include_crc = (model.vars.header_include_crc.value == False)
        whitening_match = model.vars.header_white_en.value \
                          == model.vars.payload_white_en.value
        excludewcnt_match = model.vars.header_excludesubframewcnt_en.value \
                            == model.vars.payload_excludesubframewcnt_en.value

        return (header_no_trail and header_no_include_crc and whitening_match \
                and excludewcnt_match)

    def _combine_rx_fcds_no_crc(self, model):
        """_combine_rx_fcds_no_crc

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        header_no_calccrc = (model.vars.header_calc_crc.value == False)
        header_fits_in_skipcrc = model.vars.header_size_internal.value <= 3

        return (self._combine_rx_fcds(model) and header_no_calccrc \
                and header_fits_in_skipcrc)

    def _combine_rx_fcds_no_change(self, model):
        """_combine_rx_fcds_no_change

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        calccrc_match = (model.vars.header_calc_crc.value \
                         == model.vars.payload_crc_en.value)

        return (self._combine_rx_fcds(model) and calccrc_match)

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
        if not self._combine_rx_fcds_no_crc(model) \
           and not self._combine_rx_fcds_no_change(model):
            self._configure_fcd(model, fcdindex="2", buf=1, **fcdDict)
        return

    def _configure_payload_with_header(self, model):
        """_configure_payload_with_header

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        fcdDict = {
            "excludesubframewcnt": int(model.vars.payload_excludesubframewcnt_en.value == True),
            "addtrailtxdata": int(model.vars.payload_addtrailtxdata_en.value == True),
            "skipwhite": int(model.vars.payload_white_en.value == False),
            "skipcrc": 0,
            "calccrc": int(model.vars.payload_crc_en.value == True),
            "includecrc": int(model.vars.payload_crc_en.value == True),
            "words": 0xFF,
        }
        self._configure_fcd(model, fcdindex="1", buf=0, **fcdDict)

        self._configure_fcd(model, fcdindex="3", buf=1, **fcdDict)

        if self._combine_rx_fcds_no_crc(model):
            fcdDict["skipcrc"] = model.vars.header_size_internal.value
            self._configure_fcd(model, fcdindex="2", buf=1, **fcdDict)
        elif self._combine_rx_fcds_no_change(model):
            self._configure_fcd(model, fcdindex="2", buf=1, **fcdDict)
        return