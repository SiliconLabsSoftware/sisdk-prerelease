'''
Created on Jan 29, 2013

@author: sesuskic
'''

import math
from .trueround import trueround

__all__ = ["freq_ctl"]

def _elementwiseAnd(*args):
    return [all(tup) for tup in zip(*args)]

def freq_ctl(modem_calc, rev_c0=False):
    

    ''' %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% '''
    vco_proc_var = 1.0

    fc_d = math.floor(modem_calc.inputs.API_fc/modem_calc.inputs.API_freq_xo*2.0**19) - 2**21
    
    if ( modem_calc.inputs.API_Chip_Version==0 or modem_calc.inputs.API_Chip_Version==2 or rev_c0): # normal banded part: v0
        vco_max = 4.2e9 * vco_proc_var
        vco_min = 3.4e9 * vco_proc_var
    else :
        vco_max = 3841.3215e6 * vco_proc_var
        vco_min = 2817.1845e6 * vco_proc_var

    OUTDIV = sorted(modem_calc.modulator.OUTDIV_SY_SEL_MAP)
    fmin = [vco_min/x for x in OUTDIV]
    fmax = [vco_max/x for x in OUTDIV]
    
    if rev_c0:
        fmin[1] = 420e6  # div-8 covers [420, 525] MHz per si4440-1618 & Dave's email 8/8/2014




    if ( ( modem_calc.inputs.API_Chip_Version==0 or modem_calc.inputs.API_Chip_Version==2) and not(rev_c0)) : #and (outdiv==6) )
        fmax[1] = 4.02e9/6
        fmin[2] = 420e6     # added 12/17/12 per Dave S.

    fth = [0.0, 0.0, 0.0, 0.0, 0.0]
    for i in range(0,5) :
        fth[i] = ( fmin[i]+fmax[i+1] )/2


    if ( (modem_calc.inputs.API_Chip_Version==1 or modem_calc.inputs.API_Chip_Version==3) and not(rev_c0)): # continuous part boundary set in middle of overlap
        fth = [x*1e6 for x in [705.0, 475.0, 353.0, 237.5, 177.0]]

    fth_d = [ ((x/modem_calc.inputs.API_freq_xo*2**19)-2**21)/2**14 for x in fth]

    
    freq_d_coarse = fc_d/2**14


    modem_calc.modulator.fields.outdiv = OUTDIV[5]
    for i in range(4, -1, -1) :
        if freq_d_coarse >= fth_d[i] :    
            modem_calc.modulator.fields.outdiv = OUTDIV[i]

    outdiv_ord = OUTDIV.index(modem_calc.modulator.fields.outdiv)   # order of divider ratio 


    if (modem_calc.inputs.API_hi_pfm_div_mode) : # means the fbdivider pre-scaler is div/2 otherwise div/4
        ps_fbdiv = 2
    else :
        ps_fbdiv = 4

    fc_d = (modem_calc.modulator.fields.outdiv * modem_calc.inputs.API_fc) /modem_calc.inputs.API_freq_xo*2**19

    modem_calc.modulator.fields.fbdiv_int = int(math.floor( fc_d/ps_fbdiv/2**19 ))
    fbdiv_frac_fp = fc_d/ps_fbdiv/2**19 - modem_calc.modulator.fields.fbdiv_int
    modem_calc.modulator.fields.fbdiv_frac_19b = math.floor(fbdiv_frac_fp*2**19)

    modem_calc.modulator.fields.fhst_16b = trueround((modem_calc.inputs.API_fhst * float(modem_calc.modulator.fields.outdiv)) / (ps_fbdiv * modem_calc.inputs.API_freq_xo) * 2**19)





