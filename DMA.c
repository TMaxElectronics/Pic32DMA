
#include <xc.h>
#include <stdint.h>
#include <sys/kmem.h>

#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "DMA.h"

uint32_t DMA_available[DMA_CHANNELCOUNT] = {[0 ... (DMA_CHANNELCOUNT-1)] = 1};

static uint32_t populateHandle(DMA_HANDLE_t * handle, uint32_t ch);

uint32_t DMA_setSrcConfig(DMA_HANDLE_t * handle, uint32_t * src, int32_t size){
    DCHSSA = KVA_TO_PA(src);
    DCHSSIZ = size;
}

uint32_t DMA_setDestConfig(DMA_HANDLE_t * handle, uint32_t * dest, int32_t size){
    DCHDSA = KVA_TO_PA(dest);
    DCHDSIZ = size;
}

uint32_t DMA_setTransferAttributes(DMA_HANDLE_t * handle, int32_t cellSize, int32_t startISR, int32_t abortISR){
    if(cellSize != -1){
        DCHCSIZ = cellSize;
    }
    
    uint32_t temp = DCHECON;
    
    if(startISR == -1){ //disabled
        temp &= ~_DCH0ECON_SIRQEN_MASK;
    }else{
        temp |= _DCH0ECON_SIRQEN_MASK;
        temp &= ~_DCH0ECON_CHSIRQ_MASK;
        temp |= startISR << _DCH0ECON_CHSIRQ_POSITION;
    }
    
    if(abortISR == -1){ //disabled
        temp &= ~_DCH0ECON_AIRQEN_MASK;
    }else{
        temp |= _DCH0ECON_SIRQEN_MASK;
        temp &= ~_DCH0ECON_CHAIRQ_MASK;
        temp |= abortISR << _DCH0ECON_CHAIRQ_POSITION;
    }
    
    DCHECON = temp;
}

uint32_t DMA_setChannelAttributes(DMA_HANDLE_t * handle, int32_t enableChaining, int32_t chainDir, int32_t evtIfDisabled, int32_t autoEn, int32_t prio){
    uint32_t temp = DCHCON;
    if(enableChaining != -1){
        if(enableChaining) temp |= _DCH0CON_CHCHN_MASK; else temp &= ~_DCH0CON_CHCHN_MASK;
    }
    if(chainDir != -1){
        if(chainDir) temp |= _DCH0CON_CHCHNS_MASK; else temp &= ~_DCH0CON_CHCHNS_MASK;
    }
    if(evtIfDisabled != -1){
        if(evtIfDisabled) temp |= _DCH0CON_CHAED_MASK; else temp &= ~_DCH0CON_CHAED_MASK;
    }
    if(autoEn != -1){
        if(autoEn) temp |= _DCH0CON_CHAEN_MASK; else temp &= ~_DCH0CON_CHAEN_MASK;
    }
    
    if(prio != -1){
        temp &= ~_DCH0CON_CHPRI_MASK;
        temp |= prio;
    }
    
    DCHCON = temp;
}

uint32_t DMA_setInterruptConfig(DMA_HANDLE_t * handle, int32_t srcDoneEN, int32_t srcHalfEmptyEN, int32_t dstDoneEN, 
                                        int32_t dstHalfFullEN, int32_t blockDoneEN, int32_t cellDoneEN, int32_t abortEN, int32_t errorEN){
    uint32_t temp = DCHINT;
    
    if(srcDoneEN != -1){
        if(srcDoneEN) temp |= _DCH0INT_CHSDIE_MASK; else temp &= ~_DCH0INT_CHSDIE_MASK;
    }
    if(srcHalfEmptyEN != -1){
        if(srcHalfEmptyEN) temp |= _DCH0INT_CHSHIE_MASK; else temp &= ~_DCH0INT_CHSHIE_MASK;
    }
    if(dstDoneEN != -1){
        if(dstDoneEN) temp |= _DCH0INT_CHDDIE_MASK; else temp &= ~_DCH0INT_CHDDIE_MASK;
    }
    if(dstHalfFullEN != -1){
        if(dstHalfFullEN) temp |= _DCH0INT_CHDHIE_MASK; else temp &= ~_DCH0INT_CHDHIE_MASK;
    }
    if(blockDoneEN != -1){
        if(blockDoneEN) temp |= _DCH0INT_CHBCIE_MASK; else temp &= ~_DCH0INT_CHBCIE_MASK;
    }
    if(cellDoneEN != -1){
        if(cellDoneEN) temp |= _DCH0INT_CHCCIE_MASK; else temp &= ~_DCH0INT_CHCCIE_MASK;
    }
    if(abortEN != -1){
        if(abortEN) temp |= _DCH0INT_CHTAIE_MASK; else temp &= ~_DCH0INT_CHTAIE_MASK;
    }
    if(errorEN != -1){
        if(errorEN) temp |= _DCH0INT_CHERIE_MASK; else temp &= ~_DCH0INT_CHERIE_MASK;
    }
    
    DCHINT = temp;
}

inline uint32_t DMA_readISRFlags(DMA_HANDLE_t * handle){
    return DCH0INT & 0xff;
}

DMA_HANDLE_t * DMA_allocateChannel(){
    //find a free channel in the channelList
    uint32_t currCh = 0;
    for(; currCh < DMA_CHANNELCOUNT; currCh++){
        if(DMA_available[currCh]) break;
    }
    
    if(currCh == DMA_CHANNELCOUNT){ //no free channel found...
        return 0;
    }
    
    //got number of the new channel, now create handle
    DMA_HANDLE_t * handle = pvPortMalloc(sizeof(DMA_HANDLE_t));
    if(!populateHandle(handle, currCh)){ //no free channel found...
        vPortFree(handle);
        return 0;
    }
    
    DMA_available[currCh] = 0;
    
    return handle;
}

uint32_t DMA_freeChannel(DMA_HANDLE_t * handle){
    //abort also clears CHEN
    DMA_abortTransfer(handle);
    
    DMA_available[handle->moduleID] = 1;
    vPortFree(handle);
    
    return 1;
}

static uint32_t populateHandle(DMA_HANDLE_t * handle, uint32_t ch){
    handle->moduleID = ch;
    switch(ch){
#ifdef DCH0CON
        case 0:
            handle->CON = (DCHxCON_t*) &DCH0CON;
            handle->ECON = (DCHxECON_t*) &DCH0ECON;
            handle->ECONSET = &DCH0ECONSET;
            handle->INT = (DCHxINT_t*) &DCH0INT;

            handle->SSA = &DCH0SSA;
            handle->DSA = &DCH0DSA;

            handle->SSIZ = &DCH0SSIZ;
            handle->DSIZ = &DCH0DSIZ;
            handle->CSIZ = &DCH0CSIZ;

            handle->SPTR = &DCH0SPTR;
            handle->DPTR = &DCH0DPTR;
            handle->CPTR = &DCH0CPTR;

            handle->DAT = &DCH0DAT;
            return 1;
#endif
#ifdef DCH1CON
        case 1:
            handle->CON = (DCHxCON_t*)&DCH1CON;
            handle->ECON = (DCHxECON_t*)&DCH1ECON;
            handle->ECONSET = &DCH1ECONSET;
            handle->INT = (DCHxINT_t*)&DCH1INT;

            handle->SSA = &DCH1SSA;
            handle->DSA = &DCH1DSA;

            handle->SSIZ = &DCH1SSIZ;
            handle->DSIZ = &DCH1DSIZ;
            handle->CSIZ = &DCH1CSIZ;

            handle->SPTR = &DCH1SPTR;
            handle->DPTR = &DCH1DPTR;
            handle->CPTR = &DCH1CPTR;

            handle->DAT = &DCH1DAT;
            return 1;
#endif
#ifdef DCH2CON
        case 2:
            handle->CON = (DCHxCON_t*)&DCH2CON;
            handle->ECON = (DCHxECON_t*)&DCH2ECON;
            handle->ECONSET = &DCH2ECONSET;
            handle->INT = (DCHxINT_t*)&DCH2INT;

            handle->SSA = &DCH2SSA;
            handle->DSA = &DCH2DSA;

            handle->SSIZ = &DCH2SSIZ;
            handle->DSIZ = &DCH2DSIZ;
            handle->CSIZ = &DCH2CSIZ;

            handle->SPTR = &DCH2SPTR;
            handle->DPTR = &DCH2DPTR;
            handle->CPTR = &DCH2CPTR;

            handle->DAT = &DCH2DAT;
            return 1;
#endif
#ifdef DCH3CON
        case 3:
            handle->CON = (DCHxCON_t*)&DCH3CON;
            handle->ECON = (DCHxECON_t*)&DCH3ECON;
            handle->ECONSET = &DCH3ECONSET;
            handle->INT = (DCHxINT_t*)&DCH3INT;

            handle->SSA = &DCH3SSA;
            handle->DSA = &DCH3DSA;

            handle->SSIZ = &DCH3SSIZ;
            handle->DSIZ = &DCH3DSIZ;
            handle->CSIZ = &DCH3CSIZ;

            handle->SPTR = &DCH3SPTR;
            handle->DPTR = &DCH3DPTR;
            handle->CPTR = &DCH3CPTR;

            handle->DAT = &DCH3DAT;
            return 1;
#endif
#ifdef DCH4CON
        case 4:
            handle->CON = (DCHxCON_t*)&DCH4CON;
            handle->ECON = (DCHxECON_t*)&DCH4ECON;
            handle->ECONSET = &DCH4ECONSET;
            handle->INT = (DCHxINT_t*)&DCH4INT;

            handle->SSA = &DCH4SSA;
            handle->DSA = &DCH4DSA;

            handle->SSIZ = &DCH4SSIZ;
            handle->DSIZ = &DCH4DSIZ;
            handle->CSIZ = &DCH4CSIZ;

            handle->SPTR = &DCH4SPTR;
            handle->DPTR = &DCH4DPTR;
            handle->CPTR = &DCH4CPTR;

            handle->DAT = &DCH4DAT;
            return 1;
#endif
#ifdef DCH5CON
        case 5:
            handle->CON = (DCHxCON_t*)&DCH5CON;
            handle->ECON = (DCHxECON_t*)&DCH5ECON;
            handle->ECONSET = &DCH5ECONSET;
            handle->INT = (DCHxINT_t*)&DCH5INT;

            handle->SSA = &DCH5SSA;
            handle->DSA = &DCH5DSA;

            handle->SSIZ = &DCH5SSIZ;
            handle->DSIZ = &DCH5DSIZ;
            handle->CSIZ = &DCH5CSIZ;

            handle->SPTR = &DCH5SPTR;
            handle->DPTR = &DCH5DPTR;
            handle->CPTR = &DCH5CPTR;

            handle->DAT = &DCH5DAT;
            return 1;
#endif
#ifdef DCH6CON
        case 6:
            handle->CON = (DCHxCON_t*)&DCH6CON;
            handle->ECON = (DCHxECON_t*)&DCH6ECON;
            handle->ECONSET = &DCH6ECONSET;
            handle->INT = (DCHxINT_t*)&DCH6INT;

            handle->SSA = &DCH6SSA;
            handle->DSA = &DCH6DSA;

            handle->SSIZ = &DCH6SSIZ;
            handle->DSIZ = &DCH6DSIZ;
            handle->CSIZ = &DCH6CSIZ;

            handle->SPTR = &DCH6SPTR;
            handle->DPTR = &DCH6DPTR;
            handle->CPTR = &DCH6CPTR;

            handle->DAT = &DCH6DAT;
            return 1;
#endif
#ifdef DCH7CON
        case 7:
            handle->CON = (DCHxCON_t*)&DCH7CON;
            handle->ECON = (DCHxECON_t*)&DCH7ECON;
            handle->ECONSET = &DCH7ECONSET;
            handle->INT = (DCHxINT_t*)&DCH7INT;

            handle->SSA = &DCH7SSA;
            handle->DSA = &DCH7DSA;

            handle->SSIZ = &DCH7SSIZ;
            handle->DSIZ = &DCH7DSIZ;
            handle->CSIZ = &DCH7CSIZ;

            handle->SPTR = &DCH7SPTR;
            handle->DPTR = &DCH7DPTR;
            handle->CPTR = &DCH7CPTR;

            handle->DAT = &DCH7DAT;
            return 1;
#endif
        default:
            return 0;
    }
}

inline uint32_t DMA_isBusy(DMA_HANDLE_t * handle){
    return DCHCON & _DCH0CON_CHBUSY_MASK;
}

inline uint32_t DMA_isEnabled(DMA_HANDLE_t * handle){
    return DCHCON & _DCH0CON_CHEN_MASK;
}

inline void DMA_setEnabled(DMA_HANDLE_t * handle, uint32_t en){
    DCHCONbits.CHEN = en;
}

inline void DMA_forceTransfer(DMA_HANDLE_t * handle){
    DCHECONSET = _DCH0ECON_CFORCE_MASK;
}

inline void DMA_abortTransfer(DMA_HANDLE_t * handle){
    DCHECONSET = _DCH0ECON_CABORT_MASK;
}