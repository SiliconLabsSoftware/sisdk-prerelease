'''
Created on Jan 28, 2013

@author: sesuskic
'''

import math
from .decode_api import get_ramp_dly, engfsk, get_df, enook
from .trueround import trueround
from .pro2modemfields import Pro2ModulatorFields
from .freq_ctl import freq_ctl
from .pll_setting_LUT import pll_setting_lut

class Pro2ModulatorCore(object):
    OUTDIV_SY_SEL_MAP = {4:0, 6:1, 8:2, 12:3, 16:4, 24:5}

    def __init__(self):
        self.fields = Pro2ModulatorFields()
    
    def _init_pa_tc(self, modem_calc):
        if ( modem_calc.inputs.API_modulation_type == 1 ) :
            cTC = 32 - math.ceil( 20*modem_calc.inputs.API_Rsymb/0.075e6 )
            cTC = min(31, max(2, cTC))
            modem_calc.inputs.API_TC = cTC
            fsk_mod_dly = 2  # default value, no effect in OOK
        else : # (G)FSK 
            if ( modem_calc.inputs.API_TC<0 or modem_calc.inputs.API_TC>31 ) :
                modem_calc.warning2log += '# api_tc={:d} is out of valid range [0,31].  Replaced by default value api_tc=29\n'.format(modem_calc.inputs.API_TC)
                modem_calc.inputs.API_TC = 29
    
            if     (modem_calc.inputs.API_TC<=25) :
                fsk_mod_dly = 0
            elif (modem_calc.inputs.API_TC==26) :
                fsk_mod_dly = (modem_calc.inputs.API_Rsymb >= 250e3)*1
            elif (modem_calc.inputs.API_TC==27) :
                fsk_mod_dly = (modem_calc.inputs.API_Rsymb >= 170e3)*1
            elif (modem_calc.inputs.API_TC==28) :
                fsk_mod_dly = ( (modem_calc.inputs.API_Rsymb>=170e3) and (modem_calc.inputs.API_Rsymb<250e3) )*1 + (modem_calc.inputs.API_Rsymb>=250e3)*2
            elif (modem_calc.inputs.API_TC>=29) :
                fsk_mod_dly = 1 + (modem_calc.inputs.API_Rsymb >= 170e3)*1
         
        self.fields.PA_TC = int(fsk_mod_dly*2**5 + modem_calc.inputs.API_TC)   # form API property calculate_pa_tc
    
    def _init_txosrx(self, modem_calc):
        ''' TX GLPF interpolator configuration for Rev B chip only '''
        if ( (modem_calc.inputs.API_Chip_Version==0 or modem_calc.inputs.API_Chip_Version==1) or not(engfsk(modem_calc.inputs.API_modulation_type))) :
            self.fields.txosrx2 = 0
            self.fields.txosrx4 = 0 
        elif (modem_calc.inputs.API_Rsymb > 200000) :
            self.fields.txosrx2 = 0
            self.fields.txosrx4 = 0 
        elif (modem_calc.inputs.API_Rsymb < 25000) :
            self.fields.txosrx2 = 0
            self.fields.txosrx4 = 1
        else :
            self.fields.txosrx2 = 1
            self.fields.txosrx4 = 0

    def _get_freq_ctl(self, modem_calc):
        freq_ctl(modem_calc)

    def calculate(self, modem_calc):
        self.df = get_df(modem_calc.inputs.API_modulation_type, modem_calc.inputs.API_Fdev)
        pll_setting_lut(modem_calc)
        self._init_pa_tc(modem_calc)
        self._init_txosrx(modem_calc)
        self.fields.w_size = modem_calc.w_size


        self.fields.modtype = int(modem_calc.inputs.API_dsource*8 + modem_calc.inputs.API_modulation_type)
        self.fields.mapctrl = int(modem_calc.inputs.API_Manchester*128) # inv_rxbit.format(inv_txbit.format(inv_fd.format(inv_adcq only change by explicit request
        self._get_freq_ctl(modem_calc)
    
    
        if (modem_calc.inputs.API_hi_pfm_div_mode) : 
            self.fields.dsm_ratio = modem_calc.inputs.API_freq_xo/(2**18* self.fields.outdiv) # Hz/bit 
        else :
            self.fields.dsm_ratio = modem_calc.inputs.API_freq_xo/(2**17* self.fields.outdiv) # Hz/bit
    
        ''' modulator's register calculator '''
        self.fields.fc_inte = self.fields.fbdiv_int-1 # send to frac-N divider
        self.fields.fc_frac = int(trueround(self.fields.fbdiv_frac_19b)+2**19)
        self.fields.DSM_CTRL = 7   # change default of dsm_lsb to 1 from 0
        
        self.fields.sy_sel_Band = self.OUTDIV_SY_SEL_MAP.get(self.fields.outdiv, 5)
    
        self.fields.sy_sel_Band = int(modem_calc.inputs.API_hi_pfm_div_mode*2**3 + self.fields.sy_sel_Band)
    
    
        if (modem_calc.inputs.API_hi_pfm_div_mode ==1) :
            self.fields.vco_cali_count_tx = self.fields.fc_inte*1*self.fields.w_size +  trueround(1*self.fields.w_size*self.fields.fc_frac/2**19)                                                 
        else :
            self.fields.vco_cali_count_tx = self.fields.fc_inte*2*self.fields.w_size +  trueround(2*self.fields.w_size*self.fields.fc_frac/2**19)                                                 
    
        mod_source = modem_calc.inputs.API_dsource % 4
        TX_DIRECT_MODE_TYPE = math.floor(modem_calc.inputs.API_dsource/2**4)   # 1: async; 0: sync
    
        if ( (mod_source==1) and  (TX_DIRECT_MODE_TYPE==1) ) :
            self.fields.txncomod = modem_calc.inputs.API_freq_xo
            self.fields.txdr = 500000
        else : 
            self.fields.txncomod = modem_calc.inputs.API_freq_xo
            self.fields.txdr = modem_calc.inputs.API_Rsymb*10
    
        if (self.fields.txosrx2 ==1) : 
            self.fields.txdr = 2*self.fields.txdr
        elif (self.fields.txosrx4 ==1) :
            self.fields.txdr = 4*self.fields.txdr
        
        self.fields.txfd = int(min(max(0, trueround(self.df/self.fields.dsm_ratio)), 2**17-1)) # 17-bit
    
    
        self.fields.freq_dev  = int(mod_source*2**20 + modem_calc.inputs.API_modulation_type*2**17 + self.fields.txfd)
    
        eninv_rxbit = 0
        eninv_txbit = 0
        eninv_fd = 0
        self.fields.txncomod_reg = int(modem_calc.inputs.API_Manchester*2**31+eninv_rxbit*2**30+eninv_txbit*2**29+eninv_fd*2**28+self.fields.txosrx2*2**27+self.fields.txosrx4*2**26+self.fields.txncomod)
        self.fields.hbncomod = int(self.fields.txosrx2*2**27+self.fields.txosrx4*2**26+self.fields.txncomod)
    
        if(modem_calc.inputs.API_Rsymb <= 5000) :
            ook_zero_IF_df = modem_calc.inputs.API_Rsymb*6
            self.fields.ook_zero_IF_txdr = 4*self.fields.txdr
        elif ( modem_calc.inputs.API_Rsymb > 5000 and modem_calc.inputs.API_Rsymb <= 40000) :
            ook_zero_IF_df =   30000
            self.fields.ook_zero_IF_txdr = 20000+2*self.fields.txdr
        elif ( modem_calc.inputs.API_Rsymb > 40000 and modem_calc.inputs.API_Rsymb <= 100000) :
            ook_zero_IF_df =   modem_calc.inputs.API_Rsymb*1.5
            self.fields.ook_zero_IF_txdr = self.fields.txdr
        else :
            ook_zero_IF_df   =  150000
            self.fields.ook_zero_IF_txdr = self.fields.txdr    
    
        self.fields.ook_zero_IF_txfd     = min(max(0, trueround(ook_zero_IF_df/self.fields.dsm_ratio)), 2**17-1) # 17-bit
    
        self.fields.ramp_dly = get_ramp_dly(modem_calc.inputs.API_modulation_type, modem_calc.inputs.API_if_mode)
    
    
        self.field_map = self.fields.get_field_map()

class Pro2PlusModulatorCore(Pro2ModulatorCore):
    OUTDIV_SY_SEL_MAP = {4:0, 8:2, 10:1, 12:3, 16:4, 24:5}
    
    def __init__(self):
        super(Pro2PlusModulatorCore, self).__init__()

    def _get_freq_ctl(self, modem_calc):
        freq_ctl(modem_calc, True)

    def _init_pa_tc(self, modem_calc):
        modtyp = modem_calc.inputs.API_modulation_type


        LUT = [56.0,26.0,16.0,12.0,10.0,8.0,7.0,6.0,5.0,4.4,4.0,3.6,3.2,3.0,2.8,2.6,2.5,2.4,2.2,2.0,1.9,1.9,1.8,1.8,1.6,1.5,1.5,1.4,1.3,1.3,1.2,1.1]

        if (modtyp==1) : # ook
            cTC = 32 - math.ceil( 20*modem_calc.inputs.API_Rsymb/0.075e6 )
            cTC = min(31, max(2, cTC))
            modem_calc.inputs.API_TC = cTC
            fsk_mod_dly = 2  # default value, no effect in OOK
        else : # (G)FSK 
            dbase = 10**6/modem_calc.inputs.API_Rsymb/10
            if (modtyp==2): #2fsk
                dig_delay = 2.5*dbase
            elif (modtyp==3): #2gfsk
                dig_delay = 11.5*dbase
            elif (modtyp==4): #4fsk
                dig_delay = 12.5*dbase
            elif (modtyp==5): #4gfsk
                dig_delay = 21.5*dbase
            else:  # default to 3=2GFSK
                dig_delay = 11.5*dbase
            pa_ramp_up_time = LUT[31-int(modem_calc.inputs.API_TC)]
            t_m_st_cal = (pa_ramp_up_time - dig_delay - self.fields.pll_delay-2)/2 #2us per step
            if(abs(t_m_st_cal)<1): ##min. gap time between modulator enable and pa ramp up
                t_m_st_req = 1
            elif(t_m_st_cal<0):
                t_m_st_req = 0
            else :
                t_m_st_req = t_m_st_cal
            t_m_st = min(15, t_m_st_req)
            fsk_mod_dly = min(7, math.floor(t_m_st/2))
            tmp = trueround(self.fields.pll_delay/2)
            if (tmp < fsk_mod_dly):
                self.fields.close_hw_dly_comp = fsk_mod_dly + 1
            else :
                self.fields.close_hw_dly_comp = tmp

        self.fields.PA_TC = int(fsk_mod_dly*2**5 + modem_calc.inputs.API_TC)   # form API property calculate_pa_tc





