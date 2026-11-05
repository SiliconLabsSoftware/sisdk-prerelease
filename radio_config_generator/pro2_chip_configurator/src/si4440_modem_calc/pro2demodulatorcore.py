'''
Created on Jan 28, 2013

@author: sesuskic
'''

import math
from .filter_chain_lu import FilterChainLu
from .ch_flt import ch_flt
from .decode_api import enook, en2fsk, en4gfsk, engfsk, get_fixed_if_mode, get_zero_if, ook_zero_IF, get_df 
from .trueround import trueround
from .pro2modemfields import Pro2DemodulatorFields

__all__ = ["Pro2DemodulatorCore"]

class Pro2DemodulatorCore(object):
    def __init__(self):
        self.fields = Pro2DemodulatorFields()
        self._init_decim_table()
        
    def _init_decim_table(self):
        self.decim_table = [1]
        for x in range(1,8) :
            self.decim_table.append(2.0**x)
            self.decim_table.append(3*2.0**(x-1)) 
        
    def _init_tx_rx_err(self, modem_calc):
        if (modem_calc.inputs.API_inputBW) :
            self.fields.tx_rx_err = (modem_calc.inputs.API_RXBW - self.BW_mod)/2 
        else :
            self.fields.tx_rx_err = modem_calc.inputs.API_fc*modem_calc.inputs.API_crystal_tol*2/10**6
    


    def _init_BW_mod(self, modem_calc):
        self.BW_mod = modem_calc.inputs.API_Rsymb+2*get_df(modem_calc.inputs.API_modulation_type, modem_calc.inputs.API_Fdev) # modulation BW (Hz)
        
    def _init_Ch_Fil_Bw(self, modem_calc):
        if (self.fields.nonstdpk==0) :
            if (modem_calc.inputs.API_afc_en ==1 or modem_calc.inputs.API_afc_en ==2) :
                afc_factor = 0.7    # fit from test results of h=1
            else :
                afc_factor = 0.5
        else :
            if (modem_calc.inputs.API_afc_en ==1 or modem_calc.inputs.API_afc_en ==2) :
                afc_factor = 0.1    # originally always disable PLL-FB
            else :   
                afc_factor = 0.1    # originally always disable PLL-FB
    
        max_freq_error = 2*self.fields.tx_rx_err # Hz (evaled from API_RXBW-modBW in inputBW==1)
        
        if (self.BW_mod>9e5) : 
            fsk_rx_BW = 9e5
        elif (max_freq_error > afc_factor*self.BW_mod) :
            fsk_rx_BW = max_freq_error + (1-afc_factor)*self.BW_mod
        else :
            fsk_rx_BW = self.BW_mod
            
        if (enook(modem_calc.inputs.API_modulation_type) or modem_calc.inputs.API_inputBW) : 
            self.Ch_Fil_Bw = modem_calc.inputs.API_RXBW/1000   # unit kHz
        else :
            self.Ch_Fil_Bw = fsk_rx_BW/1000
            


    def _init_force_conditions(self, modem_calc):
        self.force_condition1 = modem_calc.inputs.API_Fdev >= modem_calc.inputs.API_Rsymb and self.fields.nonstdpk == 0 and en2fsk(modem_calc.inputs.API_modulation_type)
        self.force_condition2 = self.fields.tx_rx_err > 16 * float(modem_calc.inputs.API_Rsymb) * 0.75 and self.fields.nonstdpk == 0 and en2fsk(modem_calc.inputs.API_modulation_type)

    def _init_preamble_pattern(self, modem_calc) :
        self.fields.pm_pattern = 0
        self.fields.nonstdpk = 0
        self.fields.detector = 0
        self.fields.skip_pmdet_ph = 0 
        self.fields.skipsyn = 0
        self.fields.preath = 0 
        self.fields.gear_sw = 0
    
        if (modem_calc.inputs.API_pm_pattern == 10) :    # EZRadio2 FIFO Mode: MA detector w/ pmdet+syncdet.  Packet: 4B '1010' pm + 2B sync + payload + CRC
            self.fields.pm_pattern = 0 # modem does pmdet
            self.fields.preath   = 20          # pmdet TH
            self.fields.skip_pmdet_ph = 1      # packet handler skips pmdet
            self.fields.skipsyn  = 0           # 1=skip sync-word search timeout reset
            self.fields.gear_sw  = 0           # AFC switches to lower gear after the pmdet
            self.fields.nonstdpk = 1           # non std packet 
            if (enook(modem_calc.inputs.API_modulation_type)) :              # PK detector in OOK for short preamble
                self.fields.detector = 1-modem_calc.inputs.API_Manchester    # in WDS restrict Manchester to 0
            else :
                self.fields.detector = 2       # MA detector in FSK
        elif (modem_calc.inputs.API_pm_pattern in [0,4]) :  # 1010 PM pattern: Min pm length = 32 or 40 bits
            self.fields.pm_pattern = 0    # modem does pmdet
            self.fields.preath   = 20          # pmdet TH
            self.fields.skip_pmdet_ph = 1      # packet handler skips pmdet
            self.fields.skipsyn  = 0           # 1=skip sync-word search timeout reset
            self.fields.gear_sw  = 0           # AFC switches to lower gear after the pmdet
            self.fields.nonstdpk = 0           # standard packet
            self.fields.detector = 3           # min-max detector
            if (enook(modem_calc.inputs.API_modulation_type)) :
                self.fields.detector = 0
            if (modem_calc.inputs.API_ant_div==1) :  # AntDiv requires long '1010' preamble
                self.fields.preath = 24 
        elif (modem_calc.inputs.API_pm_pattern in [1,5]) :  # 11111111 PM pattern: Min pm length = 32 or 40 bits
            self.fields.pm_pattern = 1 
            self.fields.skip_pmdet_ph = 1
            self.fields.skipsyn = 1
            self.fields.gear_sw = 0            # AFC switches to lower gear after sync word detection
            self.fields.nonstdpk = 1           # non std packet
            if (enook(modem_calc.inputs.API_modulation_type)) :
                self.fields.preath = 0
                self.fields.detector = 1-modem_calc.inputs.API_Manchester
            else :
                self.fields.preath = 16
                self.fields.detector = 3
        elif (modem_calc.inputs.API_pm_pattern in [2,6]) :    #00000000 PM pattern: Min pm length = 32 or 40 bits
            self.fields.pm_pattern = 2 
            self.fields.skip_pmdet_ph = 1
            self.fields.skipsyn = 1
            self.fields.gear_sw = 0
            self.fields.nonstdpk = 1           # non std packet 
            if (enook(modem_calc.inputs.API_modulation_type)) :
                self.fields.preath = 0
                self.fields.detector = 1-modem_calc.inputs.API_Manchester
            else :
                self.fields.preath = 16
                self.fields.detector = 3
        elif (modem_calc.inputs.API_pm_pattern in [3,7]) : #random. Min pm length = 16? 32 bits; PH can provide pmdet
            self.fields.pm_pattern = 3 
            self.fields.preath = 0  
            self.fields.skip_pmdet_ph = 0
            self.fields.skipsyn = 1
            self.fields.gear_sw = 0
            self.fields.nonstdpk = 1           # non std packet 
            if (enook(modem_calc.inputs.API_modulation_type)) :
                self.fields.detector = 1-modem_calc.inputs.API_Manchester
            elif (modem_calc.inputs.API_Manchester) : 
                self.fields.detector = 2
            else :
                self.fields.detector = 3
        elif (modem_calc.inputs.API_pm_pattern == 11) :    # EZRadio2 Direct Mode: min-max detector w/o pmdet but w/ syncdet.  
            self.fields.pm_pattern = 0 # modem does pmdet
            self.fields.preath   =  0          # pmdet TH
            self.fields.skip_pmdet_ph = 1      # packet handler skips pmdet
            self.fields.skipsyn  = 1           # skip sync-word search timeout reset
            self.fields.gear_sw  = 0           # AFC/BCR switches to lower gear after pmdet
            self.fields.nonstdpk = 1 
            if (enook(modem_calc.inputs.API_modulation_type)) :
                self.fields.detector = 1       # PK detector in OOK for short preamble
            else :
                self.fields.detector = 3       # MinMax detector in FSK
        elif (modem_calc.inputs.API_pm_pattern == 13) :    #1010 PM pattern: Min pm length = 32 or 40 bits
            self.fields.pm_pattern = 0    # modem does pmdet
            self.fields.preath   = 1*2**6;       # skip pmdet detection
            self.fields.skip_pmdet_ph = 1      # packet handler skips pmdet
            self.fields.skipsyn  = 1           # 1=skip sync-word search timeout reset
            self.fields.gear_sw  = 2           # AFC switches to lower gear after the pmdet
            self.fields.nonstdpk = 0           # standard packet
            self.fields.detector = 3           # min-max detector
            if (enook(modem_calc.inputs.API_modulation_type)) :
                self.fields.detector = 0
            if (modem_calc.inputs.API_ant_div==1) :    # AntDiv requires long '1010' preamble
                self.fields.preath = 24 
        else : # default to 15 (for pm length <16 bits); PH can NOT give pmdet
            self.fields.pm_pattern = 3 # random pm, if pmdet is done, it's by PH
            self.fields.preath = 0             # pmdet TH
            self.fields.skip_pmdet_ph = 1      # packet handler skips pmdet
            self.fields.skipsyn = 1            # skip sync-word search timeout reset
            self.fields.gear_sw = 1            # AFC/BCR switches to lower gear after syncdet 
            self.fields.nonstdpk = 1           # non std packet
            if (enook(modem_calc.inputs.API_modulation_type)) : 
                self.fields.detector = 1-modem_calc.inputs.API_Manchester
            elif (modem_calc.inputs.API_Manchester) : 
                self.fields.detector = 2
            else :
                self.fields.detector = 3

    def _pre_calc_setup(self, modem_calc):
        self.fields.bw_peak = 12
        self._init_BW_mod(modem_calc)
        self._init_tx_rx_err(modem_calc)
        self._init_preamble_pattern(modem_calc)
        if (modem_calc.inputs.API_BER_mode == 1):
            self.fields.skipsyn = 1
            self.fields.preath = 4
            
        self.fields.zero_if = get_zero_if(modem_calc.inputs.API_if_mode)
        self.fields.fixed_if_mode = get_fixed_if_mode(modem_calc.inputs.API_if_mode)
        self.fields.df = get_df(modem_calc.inputs.API_modulation_type, modem_calc.inputs.API_Fdev)
        self.fields.thd4gfsk = min(0xffff, trueround((2**15*modem_calc.inputs.API_Fdev)/modem_calc.inputs.API_Rsymb)) # unsigned 16-bit
        
        self.fields.hModInd = 2*modem_calc.inputs.API_Fdev/modem_calc.inputs.API_Rsymb   
        
        self._init_force_conditions(modem_calc)
        if (self.force_condition1 or self.force_condition2):
            self.fields.pm_pattern = 0
            self.fields.nonstdpk = 1

            if (self.force_condition1):
                modem_calc.warning2log += '# Modulation index is >=2, async demodulator is used \n'
            else:
                modem_calc.warning2log += '# Freqency error between TX & RX is greater than sycn-deomd spec,  async demodulator is used \n'
        self._init_Ch_Fil_Bw(modem_calc)
        
        self.OOKminOSR = 32          # minimum (BCR) OSR for OOK
        self.raweye_factor = 1.1    # nominal RAWEYE multiple

        self.BW_table = [modem_calc.inputs.API_freq_xo/30e6*x for x in [915.7, 824.5, 740.9,  661.1, 593.6, 535.4, 483.9, 436.1, 393.2, 353.2, 317.2, 282.5, 255.7, 230.2, 207.5]]


    def _pre_calc_lut(self, modem_calc):
        
        self.fields.ndec1 = min(self.filter_chain_lu.nDec, 3)
        self.fields.ndec2 = min((self.filter_chain_lu.nDec-self.fields.ndec1), 3)
        self.fields.dwn2byp = 1 - (self.filter_chain_lu.pre_dec_val == 2)
        self.fields.dwn3byp = 1 - (self.filter_chain_lu.pre_dec_val == 3)
        
        if (self.filter_chain_lu.newOSRt!= modem_calc.inputs.API_OSRtune) :
            modem_calc.warning2log +=  '# No room to step OSR by {} steps,  instead OSRtune={} steps is applied\n'.format(modem_calc.inputs.API_OSRtune, self.filter_chain_lu.newOSRt)
    
        Mod_Bw_norm = (self.BW_mod/1000)*self.filter_chain_lu.pre_dec_val*2**self.filter_chain_lu.nDec # unit is 1kHz
        tmp = [Mod_Bw_norm > x for x in self.BW_table]
    
        self.filter_k1 = self.filter_chain_lu.filter_k1
        if (en2fsk(modem_calc.inputs.API_modulation_type) and modem_calc.inputs.API_Rsymb >=300000 and self.fields.hModInd<=1 and modem_calc.inputs.API_inputBW==0) :
            self.filter_k1 = self.filter_k1+1 
    
        if ( modem_calc.inputs.API_Ch_Fil_Bw_AFC==1 and self.filter_k1 <15 and modem_calc.inputs.API_afc_en>0 and not(enook(modem_calc.inputs.API_modulation_type))):
            if (True in tmp) :
                self.filter_k2 = max(tmp.index(True), self.filter_k1)   # 09/11/2012, change 1 to self.filter_k1 s.t. k2>=k1
            else :
                self.filter_k2 = 15
        else :
            self.filter_k2 = self.filter_k1
    
        self.fields.rx1coearray = ch_flt(self.filter_k1)
        self.fields.rx2coearray = ch_flt(self.filter_k2)
        self.RX_CH_BW_k1 = self.BW_table[self.filter_k1-1]/(self.filter_chain_lu.pre_dec_val*2**self.filter_chain_lu.nDec)
        self.RX_CH_BW_k2 = self.BW_table[self.filter_k2-1]/(self.filter_chain_lu.pre_dec_val*2**self.filter_chain_lu.nDec)
        return
        
        
    def _calc_step_1(self, modem_calc):
        self.Fs_rx_CH = modem_calc.inputs.API_freq_xo/(8*self.filter_chain_lu.pre_dec_val)/2**self.filter_chain_lu.nDec; # unit is Hz
    
        self.OSR_rx_CH = self.Fs_rx_CH/modem_calc.inputs.API_Rsymb 
        if (enook(modem_calc.inputs.API_modulation_type)):   # OOK
            self.fields.ndec0 = math.floor( math.log(self.OSR_rx_CH/self.OOKminOSR, 2))  # make OSR >= 7 (#define self.OOKminOSR)
            self.fields.ndec0 = min(5, max(self.fields.ndec0, 1))  # limit to [1,5] for OOK! 
            
            if (modem_calc.inputs.API_OSRtune<0) :
                modem_calc.warning2log += '# In OOK mode, OSR step down is prohibited. API_OSRtune is reset to 0 \n'
            else : # >0
                Nndec0 = self.fields.ndec0-modem_calc.inputs.API_OSRtune
                if (Nndec0<1) :
                    newOSRt = self.fields.ndec0 -1
                    self.fields.ndec0 = 1
                    modem_calc.warning2log += '# No room to step up BCR OSR by {} steps, instead OSRtune={} steps is applied \n'.format(modem_calc.inputs.API_OSRtune, newOSRt)
                else : 
                    self.fields.ndec0 = Nndec0
        else : # FSK
            OSR_per_h = self.OSR_rx_CH/self.fields.hModInd
            if (self.fields.hModInd>=2 and not(enook(modem_calc.inputs.API_modulation_type)) and OSR_per_h>=8) :
                self.fields.ndec0 = max(min(5, trueround(math.log(self.OSR_rx_CH/96,2))), 0)  # target OSR=24 =>96
                self.raweye_factor = 1.1    # magnified RAWEYE multiple for hi self.fields.hModInd && hi OSR
            else :
                self.fields.ndec0 = 0  # originally self.ndec0=0 for FSK
    
            ''' Raw data Mode filter gain & CIC self.ndec0 '''
        self.raw_diff = 0.6**engfsk(modem_calc.inputs.API_modulation_type)*3**en4gfsk(modem_calc.inputs.API_modulation_type)*64*self.fields.hModInd/self.OSR_rx_CH;
        self.fields.rawflt_sel = 0 
    
    def _calc_step_2(self, modem_calc):
        peak_ma_in = (128*self.fields.tx_rx_err/modem_calc.inputs.API_Rsymb/self.OSR_rx_CH)+self.raw_diff;
    
        self.fields.rawgain = 3
        peak_ma_out = self.raweye_factor*peak_ma_in*(6-2*self.fields.rawflt_sel)*2**self.fields.rawgain
    
    
        ndec0_opt0 = ( self.fields.nonstdpk and self.fields.detector==3 and not(enook(modem_calc.inputs.API_modulation_type)) )
        if ( ndec0_opt0 and self.OSR_rx_CH>2**9) :
            self.fields.ndec0 = 1     # >511?  osr=ufix12.3
    
        if (self.fields.ndec0 >0) :
            if(peak_ma_out >= 2**8) : # self.rawgain \in {0,1,2,3}
                self.fields.rawgain = min(max(0, self.fields.rawgain - math.ceil((math.log((peak_ma_out/2**8), 2)))), 3)
        else :  
            if(peak_ma_out >= 2**12) :
                self.fields.rawgain = min(max(0, self.fields.rawgain - math.ceil(math.log((peak_ma_out/2**12), 2))), 3)
    
        self.raw_flt_gain = 16**( (self.fields.ndec0>0) )*(6-2*self.fields.rawflt_sel)*2**self.fields.rawgain # 10-bit
    
        self.fields.raweye = int(min(trueround(0.5*self.raweye_factor*self.raw_flt_gain*self.raw_diff), 2047)) # 11-bit
    
    def _calc_step_3(self, modem_calc):
        OSR_rx_BCR_int = self.OSR_rx_CH/2**self.fields.ndec0   
        self.fields.OSR_rx_BCR = trueround(OSR_rx_BCR_int*2**3) # 12-bit RTL BCR register with 3 bit fraction
    
        if ( self.fields.OSR_rx_BCR >= (2**12-1)  ) :  # register for BCR_OSR is ufix12.3
            modem_calc.calc_log.add_to_log('# Error: OSR={:6.2f} overflowed. Max OSR of BCR is 511. Please reduce API_OSRtune in the calculator input and try again.\n'.format(self.fields.OSR_rx_BCR/2**3), True)
        elif (self.fields.OSR_rx_BCR < 7*2**3) : # 11/29/2011: change error to warning to still enable 
            modem_calc.warning2log += '# OSR={:6.2f} OSR is too small. Please increase API_OSRtune in the calculator input and try again.\n'.format(self.fields.OSR_rx_BCR/2**3)
            self.fields.ncoff = trueround(2**22/OSR_rx_BCR_int)
        else :
            self.fields.ncoff = trueround(2**22/OSR_rx_BCR_int) # 22-bit with 6-bit fraction
    
    def _calc_step_4(self, modem_calc):
        if (modem_calc.inputs.API_pm_pattern==13) :
            self.syn_asyn_comb =1 
        else :
            self.syn_asyn_comb =0
    
        if ( (self.fields.nonstdpk==1 and self.syn_asyn_comb==0)or enook(modem_calc.inputs.API_modulation_type)) :
            self.bcr_h = 2       
        else :
            self.bcr_h = self.fields.hModInd
        
    def _calc_step_5(self, modem_calc):
        if (en4gfsk(modem_calc.inputs.API_modulation_type)) :
            self.fields.CR_gain = min(2047, trueround(2**15/(self.fields.OSR_rx_BCR*self.bcr_h))) # 11-bit
        else :
            self.fields.CR_gain = min(2047, trueround(2**17/(self.fields.OSR_rx_BCR*self.bcr_h))) # 11-bit
    
        if (modem_calc.IQ_CAL_setting!=0) : # for IQ calibration, BCR open-loop running
            self.fields.CR_gain = 0 
    
    def _calc_step_6(self, modem_calc):
        if(modem_calc.inputs.API_ant_div ==1) :  # Dec11,2012, slow down BCR gear for ANT-DIV mode
            self.fields.crfast = 1
        else :     
            self.fields.crfast = 0
        
        if (self.fields.pm_pattern == 0) :   # 1010 PM pattern: Min pm length = 32 bit
            if (modem_calc.API_Rb_error==1) :
                self.fields.rxncocomp = 1
                self.fields.crgainx2  = 0
                self.fields.crslow = 0
            elif(modem_calc.API_Rb_error==2) :  # shall remove, but caller func shall limit to 1 already
                self.fields.rxncocomp = 1
                self.fields.crgainx2  = 1
                self.fields.crslow = 1
            else :
                self.fields.rxncocomp = 0
                self.fields.crgainx2  = 0
                self.fields.crslow = 2;
        else : # non-standard pattern
            self.fields.rxncocomp = 0
            self.fields.crgainx2  = 0
            self.fields.crslow = 0  
    
    def _calc_step_7(self, modem_calc):
        if (self.fields.nonstdpk==1 or enook(modem_calc.inputs.API_modulation_type) or modem_calc.inputs.API_BER_mode==1) : 
            self.fields.bcrfbbyp = 1
            self.fields.slicefbbyp = 1
        else :
            self.fields.bcrfbbyp = 0
            self.fields.slicefbbyp = 0
    
        if (self.syn_asyn_comb==1) :
            self.fields.slicefbbyp = 0
    
        self.fields.chflt_lopw = 1 - modem_calc.inputs.API_High_perf_Ch_Fil
        if (en2fsk(modem_calc.inputs.API_modulation_type) and self.fields.hModInd >10 and self.BW_mod > 2e5) : 
            self.fields.droopfltbyp = 1 
        else :
            self.fields.droopfltbyp = 0
        self.fields.distogg = 0
        self.fields.ph0size = 0 # undefined
    

    def _calc_step_8(self, modem_calc):
        self.fields.dis_midpt = self.fields.nonstdpk

    def _calc_step_9(self, modem_calc):
        if (en4gfsk(modem_calc.inputs.API_modulation_type)) :
            self.fields.dis_midpt = 1
    
    def _calc_step_10(self, modem_calc):
        self.fields.ma_freqdwn = 0
        self.fields.sch_frzen  = 1
        self.fields.sch_frz_th = 5
        self.fields.schprd_h   = 1
        self.fields.schprd_low = 2  # 0==> 2Tb MA;1==> 4Tb MA;2==> 8Tb MA;3==> 16Tb MA;
        if (modem_calc.inputs.API_pm_pattern==17) :
            self.fields.sch_frzen = 0      # disable minmax TH freeze
            self.fields.schprd_low = 1     # search period low = sch-prd-hi, no gear switching in direct mode!
    
    def _calc_step_11(self, modem_calc):
        self.afc_gain_cal = (2*modem_calc.inputs.API_Rsymb)/modem_calc.modulator.fields.dsm_ratio # with 9 bit fraction
    
        if (modem_calc.inputs.API_Rsymb >= 100e3) :  # 100kbps RX_BW = 206kHz
            self.afc_adj = 0
        else :
            self.afc_adj = 1
    
    def _calc_step_12(self, modem_calc):
        if (self.fields.nonstdpk==1 and self.fields.detector==2 ) : # MA nonstdpk
            afc_gain_abs = 2**5*self.afc_gain_cal/self.raw_flt_gain/2/(2**self.afc_adj)    
        elif (self.fields.nonstdpk==1 and self.fields.detector==3 ) : # minmax nonstdpk
            afc_gain_abs = self.fields.OSR_rx_BCR/2**2*self.afc_gain_cal/self.raw_flt_gain
        else : # stdpk 2Tb
            afc_gain_abs =  self.afc_gain_cal/2**self.afc_adj                            
        if (modem_calc.inputs.API_ant_div==1 and (modem_calc.inputs.API_afc_en == 1 and modem_calc.inputs.API_BER_mode==0)):
            afc_gain_abs = afc_gain_abs/8

        self.afc_gain_b4satu = trueround(afc_gain_abs/2**3)      # w/ 6 bit fraction 
        self.fields.afc_gain = max(min(4095, self.afc_gain_b4satu), 1)  # range: [1, 4095]


    def _calc_orig_afclim(self, modem_calc, k_ratio, afc_RX_BW):
        orig_afclim = trueround(2 ** 6 * afc_RX_BW * 1000 * 
                                (k_ratio + (0.04 - afc_RX_BW / 206 / 100) * self.afc_adj) 
                                / self.fields.afc_gain / modem_calc.modulator.fields.dsm_ratio)
        return orig_afclim

    def _calc_step_13(self, modem_calc):
        afc_gain_satu_ratio = self.afc_gain_b4satu/self.fields.afc_gain
    
        if (afc_gain_satu_ratio > 1.0) :  # saturated
            k_ratio = 0.38
        else :
            k_ratio = 0.4
    
        if (self.fields.hModInd>=2) :
            k_ratio = k_ratio*0.714
    
        afc_RX_BW = max(self.RX_CH_BW_k1, self.Ch_Fil_Bw)
    
        orig_afclim = self._calc_orig_afclim(modem_calc, k_ratio, afc_RX_BW)  
        
        if (orig_afclim >= 2**14) :  # MSB=1 ==> 8x bits[13:0]
            self.fields.afclim = 2**14 + trueround(orig_afclim/8)
        else :
            self.fields.afclim = orig_afclim

    def _calc_step_14(self, modem_calc):
        if (modem_calc.inputs.API_ant_div==1 and modem_calc.inputs.API_afc_en ==1):
          if (modem_calc.inputs.API_Rsymb >=200000):
            self.fields.afc_shwait = 2 + self.fields.nonstdpk*2**(self.fields.schprd_h)
            self.fields.afc_lgwait = 3 + self.fields.nonstdpk*2**self.fields.schprd_low
          else :
            self.fields.afc_shwait = 1 + self.fields.nonstdpk*2**(self.fields.schprd_h);
            self.fields.afc_lgwait = 2 + self.fields.nonstdpk*2**(self.fields.schprd_low)
        else :
          if (self.afc_adj==1) : # if Rb<100kbps 4bits enough to settle PLL
            self.fields.afc_shwait = 1 + self.fields.nonstdpk*2**(self.fields.schprd_h);
            self.fields.afc_lgwait = 2 + self.fields.nonstdpk*2**(self.fields.schprd_low)
          else :
            self.fields.afc_shwait = 2 + self.fields.nonstdpk*2**(self.fields.schprd_h)
            self.fields.afc_lgwait = 3 + self.fields.nonstdpk*2**self.fields.schprd_low
    
    def _calc_step_15(self, modem_calc):
        if (modem_calc.inputs.API_BER_mode==1) : 
            self.fields.afcbd = 0                  # no filter switch in BER-mode tests
        else :
            self.fields.afcbd = modem_calc.inputs.API_Ch_Fil_Bw_AFC  # disable wide band AFC selection
    
        if (enook(modem_calc.inputs.API_modulation_type) or modem_calc.inputs.API_BER_mode==1) :
            self.fields.afc_est_en = 0
            self.fields.afc_fb_pll = 0
        else :
            self.fields.afc_est_en = 1
            self.fields.afc_fb_pll = (modem_calc.inputs.API_afc_en > 0)
    
        if (self.fields.pm_pattern==3 or self.fields.pm_pattern==15) :     
            self.fields.afc_gear_hi = 2
        else :
            self.fields.afc_gear_hi = 0
    
        self.fields.afc_gear_lo = 4*(self.fields.nonstdpk)
        self.fields.afc_freez_en = 1 #-nonstdpk
    
        measwin_cons   = 2**self.fields.ndec0*2e6 
        settlewin_cons = 2**self.fields.ndec0*25e5
    
        self.fields.measwin   = max(1,trueround(self.Fs_rx_CH/measwin_cons))   # 4-bit ,do't trunk
        self.fields.settlewin = max(1,trueround(self.Fs_rx_CH/settlewin_cons)) # 4-bit
    
        agc_cycle = self.fields.measwin + self.fields.settlewin
        self.fields.ifpd_decay = max(min(trueround((7*self.fields.OSR_rx_BCR)/agc_cycle/2**4), 255), 1) # 8-bit, limit to [1,255]
        self.fields.rfpd_decay = self.fields.ifpd_decay
    
    
        if (enook(modem_calc.inputs.API_modulation_type)) : 
            self.fields.agcovpkt = modem_calc.inputs.API_Manchester 
        else :
            self.fields.agcovpkt = 1
    
        self.fields.ifpdslow = 1
        self.fields.rfpdslow = 1
        self.fields.sgi_n = 0
        self.fields.rst_pkdt_period = 0
    
    def _calc_step_16(self, modem_calc):
        if (modem_calc.inputs.API_Manchester==0 and self.fields.pm_pattern ==0 ) :
            self.fields.ookfrz_en = 1 
        else :
            self.fields.ookfrz_en = 0
    
        self.fields.s2p_map=2
        self.fields.fast_ma = 0
        self.fields.raw_syn = 0
        self.fields.slicer_fast=1
        self.fields.Squelch=0 # 1-bit
    
        self.fields.Attack = 2
        self.fields.Decay = min(11, 4+math.ceil(math.log(self.fields.OSR_rx_BCR, 2))-3) # OSR has 3 frac bits
    
    def _calc_step_17(self, modem_calc):
        self.fields.phcompbyp = 0 # This bit should always be set to '0'
        if (engfsk(modem_calc.inputs.API_modulation_type)) :
            self.fields.phcomp_gain_4gfsk1 = self.fields.phcompbyp*2**7 + 0  # 4GFSK
            self.fields.phcomp_gain_4gfsk0 = 26  # 4GFSK
        else :
            self.fields.phcomp_gain_4gfsk1 =  self.fields.phcompbyp*2**7 + 0  # 4FSK 12/11/2012
            self.fields.phcomp_gain_4gfsk0 =  2  # 4FSK 12/11/2012
    
    def _calc_step_18(self, modem_calc):
        self.decim_ratio_iqcal = self.filter_chain_lu.decim_ratio_iqcal
        self.decimating_ratio = self.filter_chain_lu.pre_dec_val*2**self.filter_chain_lu.nDec  # 6-bit
    
        if (self.fields.fixed_if_mode) : # FIF mode
            modem_calc.if_freq = -modem_calc.inputs.API_freq_xo/64  # Hz
            modem_calc.if_freq_iqcal = modem_calc.if_freq  # Hz
            if (modem_calc.inputs.API_hi_pfm_div_mode==1) :
                self.fields.if_diff = -2**12* modem_calc.modulator.fields.outdiv   # signed 18-bit 
            else :
                self.fields.if_diff = -2**11* modem_calc.modulator.fields.outdiv   # signed 18-bit
        elif (self.fields.zero_if): # ZIF mode
            modem_calc.if_freq = 0.0
            modem_calc.if_freq_iqcal = 0.0
            self.fields.if_diff = 0.0
        else :   # SIF mode
            modem_calc.if_freq_iqcal = -modem_calc.inputs.API_freq_xo/64/self.decim_ratio_iqcal
            modem_calc.if_freq = -modem_calc.inputs.API_freq_xo/64/self.decimating_ratio
            if (modem_calc.inputs.API_hi_pfm_div_mode==1) :
                self.fields.if_diff = -trueround( 2**12*modem_calc.modulator.fields.outdiv/self.decimating_ratio)  # signed 18-bit
            else :
                self.fields.if_diff = -trueround( 2**11*modem_calc.modulator.fields.outdiv/self.decimating_ratio)  # signed 18-bit

    
    def _calc_step_19(self, modem_calc):
        self.fields.afczifoff = 0 # set to 0 now, this value will be changed according to differnt Rb & Fd
    
        if ( modem_calc.if_freq > (-modem_calc.inputs.API_freq_xo/64) ) :  
            self.fields.realadc = 1
        else :
            self.fields.realadc = 0
    
        if (self.syn_asyn_comb==1) :
            self.fields.ph_scr_sel = 0
        else :
            self.fields.ph_scr_sel = self.fields.nonstdpk
        self.fields.en2Tb_est = 1-self.fields.nonstdpk

    def _calc_step_20(self, modem_calc):
        if (modem_calc.inputs.API_raw_demod==1) : 
            self.fields.nonstdpk_final = 1
        else:
            self.fields.nonstdpk_final = self.fields.nonstdpk
    
    def _calc_step_21(self, modem_calc):
        if (modem_calc.inputs.API_Rsymb <= 500 and enook(modem_calc.inputs.API_modulation_type)) :
            self.fields.agc_slow = 1 
        else :
            self.fields.agc_slow = 0
    
        
        temp1 = trueround(modem_calc.w_size*modem_calc.modulator.fields.outdiv/2**7)
        temp2 = trueround(modem_calc.w_size*modem_calc.modulator.fields.outdiv/2**7/self.decimating_ratio)
        
        if (self.fields.fixed_if_mode) :
            self.fields.vco_cali_count_rx = modem_calc.modulator.fields.vco_cali_count_tx - temp1 
            self.fields.vco_rx_adj = -temp1
        elif (self.fields.zero_if) :
            self.fields.vco_cali_count_rx = modem_calc.modulator.fields.vco_cali_count_tx 
            self.fields.vco_rx_adj = 0
        else :
            self.fields.vco_cali_count_rx = modem_calc.modulator.fields.vco_cali_count_tx - temp2 
            self.fields.vco_rx_adj = -temp2
        
        self._set_rxgainx2(modem_calc)
        
    def _set_rxgainx2(self, modem_calc):
        if (modem_calc.inputs.API_Rsymb >= 1e3):
            self.fields.rxgainx2 = 0
        else :
            self.fields.rxgainx2 = 1    # DFE gainx2 enabled only when Rb<1k and in PRO2
    
    def _calc_step_22(self, modem_calc):
        self.fields.en_adc_rst = 0
        self.fields.adcwatch = 0 
        self.fields.adc_gain_cor_en = 1  
        if (modem_calc.inputs.API_ant_div==1) :
            self.fields.antdiv = 4
        else :
            self.fields.antdiv = 0


    def _calc_step_23(self, modem_calc):
        self.fields.code4gfsk = 225 # 228(binnary code;%%% 180 (GrayCode);
    
        if (modem_calc.inputs.API_ant_div==1 and (modem_calc.inputs.API_afc_en == 1 and modem_calc.inputs.API_BER_mode==0)):
          self.fields.ant_mode = 2   # default: 8'd18
        else :
          self.fields.ant_mode = 1

        self.fields.enfzpmend = 0
        self.fields.enafc_clksw = 0
        self.fields.matap = 0 
        self.fields.dc_rst = 1
        self.fields.dc_freez =0       
        if(self.fields.fixed_if_mode) :
            self.fields.dc_gear = 3
            self.fields.dc_follow = 0          
        else :
            self.fields.dc_gear = 0
            self.fields.dc_follow = 1 
    
        if ( modem_calc.IQ_CAL_setting==0 ) :
            self.fields.RSSI_comp = 64 - 6*(self.fields.rxgainx2==1)     # add into RSSI value
        else :
            self.fields.RSSI_comp = 34 - 6*(self.fields.rxgainx2==1)
    
        if (self.decimating_ratio==1) :
            self.fields.fixed_if_mode = 1
    
        if(ook_zero_IF(modem_calc.inputs.API_modulation_type, modem_calc.inputs.API_if_mode)) :
            self.fields.ook_zerog = 1 # for zero-IF OOK mode, GFSK modulation mode is forced during RX
        else :
            self.fields.ook_zerog = 0 # modulation is controlled by modulation type register  
    
        self.fields.adc_hgain = 0
        self.fields.adc_en_drst = 0        # default en_drst=0
        
        self.fields.rxcomp_lat = 0
        self.fields.esc_midpt = 0
        
        self.fields.eninv_adcq = 0
        if (en4gfsk(modem_calc.inputs.API_modulation_type)) :
            self.fields.ant2pm_thd = 15 # default
        else :
            self.fields.ant2pm_thd = 8 # default
    
    def _calculate(self, modem_calc):
        self._calc_step_1(modem_calc)
        self._calc_step_2(modem_calc)
        self._calc_step_3(modem_calc)
        self._calc_step_4(modem_calc)
        self._calc_step_5(modem_calc)
        self._calc_step_6(modem_calc)
        self._calc_step_7(modem_calc)
        self._calc_step_8(modem_calc) 
        self._calc_step_9(modem_calc) 
        self._calc_step_10(modem_calc) 
        self._calc_step_11(modem_calc) 
        self._calc_step_12(modem_calc) 
        self._calc_step_13(modem_calc) 
        self._calc_step_14(modem_calc) 
        self._calc_step_15(modem_calc) 
        self._calc_step_16(modem_calc) 
        self._calc_step_17(modem_calc) 
        self._calc_step_18(modem_calc) 
        self._calc_step_19(modem_calc) 
        self._calc_step_20(modem_calc) 
        self._calc_step_21(modem_calc) 
        self._calc_step_22(modem_calc) 
        self._calc_step_23(modem_calc)
    
    def calculate(self, modem_calc) :
        self._pre_calc_setup(modem_calc)
        self.filter_chain_lu = FilterChainLu(modem_calc)
        self._pre_calc_lut(modem_calc)
        self._calculate(modem_calc)
        self.field_map = self.fields.get_field_map()
    
      
