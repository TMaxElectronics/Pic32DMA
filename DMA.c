
#include <xc.h>
#include <stdint.h>
#include <sys/attribs.h>
#include <sys/kmem.h>

#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "DMA.h"

uint32_t DMA_available[DMA_CHANNELCOUNT] = {[0 ... (DMA_CHANNELCOUNT-1)] = 1};
DMAISR_t DMA_irqHandler[DMA_CHANNELCOUNT] = {[0 ... (DMA_CHANNELCOUNT-1)].handler = NULL, [0 ... (DMA_CHANNELCOUNT-1)].handle = NULL};

static uint32_t populateHandle(DMA_HANDLE_t * handle, uint32_t ch);

uint32_t DMA_setIRQHandler(DMA_HANDLE_t * handle, DMAIRQHandler_t handlerFunction, void * data){
    DMA_irqHandler[handle->moduleID].handler = handlerFunction;
    DMA_irqHandler[handle->moduleID].data = data;
    DMA_setInterruptConfig(handle, -1, -1, -1, -1, -1, -1, -1, -1); //update IEC register without changing any module enables
}

uint32_t DMA_setSrcConfig(DMA_HANDLE_t * handle, uint32_t * src, uint32_t size){
    DCHSSA = KVA_TO_PA(src);
    DCHSSIZ = size;
}

uint32_t DMA_setDestConfig(DMA_HANDLE_t * handle, uint32_t * dest, uint32_t size){
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
    
    //is there a handler registered? If so make sure we actually enable the IRQ
    if(DMA_irqHandler[handle->moduleID].handler != NULL){
        *(handle->IECREG) |= handle->iecMask;
    }else{
        *(handle->IECREG) &= ~handle->iecMask;
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
    
    DMA_irqHandler[handle->moduleID].handler = NULL;
    DMA_irqHandler[handle->moduleID].data = NULL;
    
    DMA_setInterruptConfig(handle, -1, -1, -1, -1, -1, -1, -1, -1); //update IEC register without changing any module enables
    
    DMA_irqHandler[handle->moduleID].handle = NULL;
    DMA_available[handle->moduleID] = 1;
    vPortFree(handle);
    
    return 1;
}

static uint32_t populateHandle(DMA_HANDLE_t * handle, uint32_t ch){
    handle->moduleID = ch;
    DMA_irqHandler[handle->moduleID].handle = handle;
    switch(ch){
#ifdef DCH0CON
        case 0:
            handle->CON = (DCHxCON_t*) &DCH0CON;
            handle->ECON = (DCHxECON_t*) &DCH0ECON;
            handle->ECONSET = &DCH0ECONSET;
            handle->INT = (DCHxINT_t*) &DCH0INT;
            handle->INTCLR = &DCH0INTCLR;
            

            handle->SSA = &DCH0SSA;
            handle->DSA = &DCH0DSA;

            handle->SSIZ = &DCH0SSIZ;
            handle->DSIZ = &DCH0DSIZ;
            handle->CSIZ = &DCH0CSIZ;

            handle->SPTR = &DCH0SPTR;
            handle->DPTR = &DCH0DPTR;
            handle->CPTR = &DCH0CPTR;

            handle->DAT = &DCH0DAT;
            
            handle->IECREG = &IEC4;
            handle->iecMask = _IEC4_DMA0IE_MASK;
            
            IPC33bits.DMA0IP = 4;
            IPC33bits.DMA0IS = 3;
            
            return 1;
#endif
#ifdef DCH1CON
        case 1:
            handle->CON = (DCHxCON_t*)&DCH1CON;
            handle->ECON = (DCHxECON_t*)&DCH1ECON;
            handle->ECONSET = &DCH1ECONSET;
            handle->INT = (DCHxINT_t*)&DCH1INT;
            handle->INTCLR = &DCH1INTCLR;

            handle->SSA = &DCH1SSA;
            handle->DSA = &DCH1DSA;

            handle->SSIZ = &DCH1SSIZ;
            handle->DSIZ = &DCH1DSIZ;
            handle->CSIZ = &DCH1CSIZ;

            handle->SPTR = &DCH1SPTR;
            handle->DPTR = &DCH1DPTR;
            handle->CPTR = &DCH1CPTR;

            handle->DAT = &DCH1DAT;
            
            handle->IECREG = &IEC4;
            handle->iecMask = _IEC4_DMA1IE_MASK;
            
            IPC33bits.DMA1IP = 4;
            IPC33bits.DMA1IS = 3;
            return 1;
#endif
#ifdef DCH2CON
        case 2:
            handle->CON = (DCHxCON_t*)&DCH2CON;
            handle->ECON = (DCHxECON_t*)&DCH2ECON;
            handle->ECONSET = &DCH2ECONSET;
            handle->INT = (DCHxINT_t*)&DCH2INT;
            handle->INTCLR = &DCH2INTCLR;

            handle->SSA = &DCH2SSA;
            handle->DSA = &DCH2DSA;

            handle->SSIZ = &DCH2SSIZ;
            handle->DSIZ = &DCH2DSIZ;
            handle->CSIZ = &DCH2CSIZ;

            handle->SPTR = &DCH2SPTR;
            handle->DPTR = &DCH2DPTR;
            handle->CPTR = &DCH2CPTR;

            handle->DAT = &DCH2DAT;
            
            handle->IECREG = &IEC4;
            handle->iecMask = _IEC4_DMA2IE_MASK;
            
            IPC34bits.DMA2IP = 4;
            IPC34bits.DMA2IS = 3;
            return 1;
#endif
#ifdef DCH3CON
        case 3:
            handle->CON = (DCHxCON_t*)&DCH3CON;
            handle->ECON = (DCHxECON_t*)&DCH3ECON;
            handle->ECONSET = &DCH3ECONSET;
            handle->INT = (DCHxINT_t*)&DCH3INT;
            handle->INTCLR = &DCH3INTCLR;

            handle->SSA = &DCH3SSA;
            handle->DSA = &DCH3DSA;

            handle->SSIZ = &DCH3SSIZ;
            handle->DSIZ = &DCH3DSIZ;
            handle->CSIZ = &DCH3CSIZ;

            handle->SPTR = &DCH3SPTR;
            handle->DPTR = &DCH3DPTR;
            handle->CPTR = &DCH3CPTR;

            handle->DAT = &DCH3DAT;
            
            handle->IECREG = &IEC4;
            handle->iecMask = _IEC4_DMA3IE_MASK;
            
            IPC34bits.DMA3IP = 4;
            IPC34bits.DMA3IS = 3;
            return 1;
#endif
#ifdef DCH4CON
        case 4:
            handle->CON = (DCHxCON_t*)&DCH4CON;
            handle->ECON = (DCHxECON_t*)&DCH4ECON;
            handle->ECONSET = &DCH4ECONSET;
            handle->INT = (DCHxINT_t*)&DCH4INT;
            handle->INTCLR = &DCH4INTCLR;

            handle->SSA = &DCH4SSA;
            handle->DSA = &DCH4DSA;

            handle->SSIZ = &DCH4SSIZ;
            handle->DSIZ = &DCH4DSIZ;
            handle->CSIZ = &DCH4CSIZ;

            handle->SPTR = &DCH4SPTR;
            handle->DPTR = &DCH4DPTR;
            handle->CPTR = &DCH4CPTR;

            handle->DAT = &DCH4DAT;
            
            handle->IECREG = &IEC4;
            handle->iecMask = _IEC4_DMA4IE_MASK;
            
            IPC34bits.DMA4IP = 4;
            IPC34bits.DMA4IS = 3;
            return 1;
#endif
#ifdef DCH5CON
        case 5:
            handle->CON = (DCHxCON_t*)&DCH5CON;
            handle->ECON = (DCHxECON_t*)&DCH5ECON;
            handle->ECONSET = &DCH5ECONSET;
            handle->INT = (DCHxINT_t*)&DCH5INT;
            handle->INTCLR = &DCH5INTCLR;

            handle->SSA = &DCH5SSA;
            handle->DSA = &DCH5DSA;

            handle->SSIZ = &DCH5SSIZ;
            handle->DSIZ = &DCH5DSIZ;
            handle->CSIZ = &DCH5CSIZ;

            handle->SPTR = &DCH5SPTR;
            handle->DPTR = &DCH5DPTR;
            handle->CPTR = &DCH5CPTR;

            handle->DAT = &DCH5DAT;
            
            handle->IECREG = &IEC4;
            handle->iecMask = _IEC4_DMA5IE_MASK;
            
            IPC34bits.DMA5IP = 4;
            IPC34bits.DMA5IS = 3;
            return 1;
#endif
#ifdef DCH6CON
        case 6:
            handle->CON = (DCHxCON_t*)&DCH6CON;
            handle->ECON = (DCHxECON_t*)&DCH6ECON;
            handle->ECONSET = &DCH6ECONSET;
            handle->INT = (DCHxINT_t*)&DCH6INT;
            handle->INTCLR = &DCH6INTCLR;

            handle->SSA = &DCH6SSA;
            handle->DSA = &DCH6DSA;

            handle->SSIZ = &DCH6SSIZ;
            handle->DSIZ = &DCH6DSIZ;
            handle->CSIZ = &DCH6CSIZ;

            handle->SPTR = &DCH6SPTR;
            handle->DPTR = &DCH6DPTR;
            handle->CPTR = &DCH6CPTR;

            handle->DAT = &DCH6DAT;
            
            handle->IECREG = &IEC4;
            handle->iecMask = _IEC4_DMA6IE_MASK;
            
            IPC35bits.DMA6IP = 4;
            IPC35bits.DMA6IS = 3;
            return 1;
#endif
#ifdef DCH7CON
        case 7:
            handle->CON = (DCHxCON_t*)&DCH7CON;
            handle->ECON = (DCHxECON_t*)&DCH7ECON;
            handle->ECONSET = &DCH7ECONSET;
            handle->INT = (DCHxINT_t*)&DCH7INT;
            handle->INTCLR = &DCH7INTCLR;

            handle->SSA = &DCH7SSA;
            handle->DSA = &DCH7DSA;

            handle->SSIZ = &DCH7SSIZ;
            handle->DSIZ = &DCH7DSIZ;
            handle->CSIZ = &DCH7CSIZ;

            handle->SPTR = &DCH7SPTR;
            handle->DPTR = &DCH7DPTR;
            handle->CPTR = &DCH7CPTR;

            handle->DAT = &DCH7DAT;
            
            handle->IECREG = &IEC4;
            handle->iecMask = _IEC4_DMA7IE_MASK;
            
            IPC35bits.DMA7IP = 4;
            IPC35bits.DMA7IS = 3;
            return 1;
#endif
        default:
            return 0;
    }
}

inline void DMA_clearGloablIF(DMA_HANDLE_t * handle){
    IFS4CLR = _IFS4_DMA0IF_MASK | _IFS4_DMA1IF_MASK | _IFS4_DMA2IF_MASK | _IFS4_DMA3IF_MASK | _IFS4_DMA4IF_MASK | _IFS4_DMA5IF_MASK | _IFS4_DMA6IF_MASK | _IFS4_DMA7IF_MASK;
}

inline void DMA_clearIF(DMA_HANDLE_t * handle, uint32_t mask){
    DCHINTCLR = mask;
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

void __ISR(_DMA0_VECTOR) DMA0ISR(){
    IFS4CLR = _IFS4_DMA0IF_MASK;
    if(DMA_irqHandler[0].handler != NULL){
        (*(DMA_irqHandler[0].handler))(DCH0INT, DMA_irqHandler[0].data);
    }
}

void __ISR(_DMA1_VECTOR) DMA1ISR(){
    IFS4CLR = _IFS4_DMA1IF_MASK;
    if(DMA_irqHandler[1].handler != NULL){
        (*(DMA_irqHandler[1].handler))(DCH1INT, DMA_irqHandler[1].data);
    }
}

void __ISR(_DMA2_VECTOR) DMA2ISR(){
    IFS4CLR = _IFS4_DMA2IF_MASK;
    if(DMA_irqHandler[2].handler != NULL){
        (*(DMA_irqHandler[2].handler))(DCH2INT, DMA_irqHandler[2].data);
    }
}

void __ISR(_DMA3_VECTOR) DMA3ISR(){
    IFS4CLR = _IFS4_DMA3IF_MASK;
    if(DMA_irqHandler[3].handler != NULL){
        (*(DMA_irqHandler[3].handler))(DCH3INT, DMA_irqHandler[3].data);
    }
}

void __ISR(_DMA4_VECTOR) DMA4ISR(){
    IFS4CLR = _IFS4_DMA4IF_MASK;
    if(DMA_irqHandler[4].handler != NULL){
        (*(DMA_irqHandler[4].handler))(DCH4INT, DMA_irqHandler[4].data);
    }
}

void __ISR(_DMA5_VECTOR) DMA5ISR(){
    IFS4CLR = _IFS4_DMA5IF_MASK;
    if(DMA_irqHandler[5].handler != NULL){
        (*(DMA_irqHandler[5].handler))(DCH5INT, DMA_irqHandler[5].data);
    }
}

void __ISR(_DMA6_VECTOR) DMA6ISR(){
    IFS4CLR = _IFS4_DMA6IF_MASK;
    if(DMA_irqHandler[6].handler != NULL){
        (*(DMA_irqHandler[6].handler))(DCH6INT, DMA_irqHandler[6].data);
    }
}

void __ISR(_DMA7_VECTOR) DMA7ISR(){
    IFS4CLR = _IFS4_DMA7IF_MASK;
    if(DMA_irqHandler[7].handler != NULL){
        (*(DMA_irqHandler[7].handler))(DCH7INT, DMA_irqHandler[7].data);
    }
}