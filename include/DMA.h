#ifndef DMA_INC
#define DMA_INC

#include <xc.h>

#define DMA_ISR_DISABLED -1
#define DMA_ALL_IF _DCH0INT_CHSHIF_MASK | _DCH0INT_CHSHIF_MASK | _DCH0INT_CHDDIF_MASK | _DCH0INT_CHDHIF_MASK | _DCH0INT_CHBCIF_MASK | _DCH0INT_CHCCIF_MASK | _DCH0INT_CHTAIF_MASK | _DCH0INT_CHERIF_MASK

typedef struct __DMA_Descriptor__ DMA_HANDLE_t;
typedef void (* DMAIRQHandler_t)(uint32_t evt, void * data);

typedef struct{
    DMAIRQHandler_t    handler;
    void            *  data;
    DMA_HANDLE_t    *  handle;
} DMAISR_t;

extern inline uint32_t DMA_isBusy(DMA_HANDLE_t * handle);
extern inline uint32_t DMA_readISRFlags(DMA_HANDLE_t * handle);

extern inline uint32_t DMA_isEnabled(DMA_HANDLE_t * handle);
extern inline void DMA_setEnabled(DMA_HANDLE_t * handle, uint32_t en);

extern inline void DMA_forceTransfer(DMA_HANDLE_t * handle);
extern inline void DMA_abortTransfer(DMA_HANDLE_t * handle);
extern inline void DMA_clearGloablIF(DMA_HANDLE_t * handle);
extern inline void DMA_clearIF(DMA_HANDLE_t * handle, uint32_t mask);

uint32_t DMA_setIRQHandler(DMA_HANDLE_t * handle, DMAIRQHandler_t handlerFunction, void * data);

uint32_t DMA_setSrcConfig(DMA_HANDLE_t * handle, uint32_t * src, uint32_t size);
uint32_t DMA_setDestConfig(DMA_HANDLE_t * handle, uint32_t * dest, uint32_t size);

uint32_t DMA_setTransferAttributes(DMA_HANDLE_t * handle, int32_t cellSize, int32_t startISR, int32_t abortISR);

uint32_t DMA_setChannelAttributes(DMA_HANDLE_t * handle, int32_t enableChaining, int32_t chainDir, int32_t evtIfDisabled, int32_t autoEn, int32_t prio);
uint32_t DMA_setInterruptConfig(DMA_HANDLE_t * handle, int32_t srcDoneEN, int32_t srcHalfEmptyEN, int32_t dstDoneEN, int32_t dstHalfFullEN, int32_t blockDoneEN, int32_t cellDoneEN, int32_t abortEN, int32_t errorEN);

DMA_HANDLE_t * DMA_allocateChannel();
uint32_t DMA_freeChannel(DMA_HANDLE_t * handle);

#define DCHCON  handle->CON->w
#define DCHCONbits (*handle->CON)
#define DCHECON handle->ECON->w
#define DCHECONSET *(handle->ECONSET)
#define DCHECONbits (*handle->ECON)
#define DCHINT  handle->INT->w
#define DCHINTbits (*handle->INT)
#define DCHINTCLR  *(handle->INTCLR)

#define DCHSSA  *(handle->SSA)
#define DCHDSA  *(handle->DSA)
#define DCHSSIZ *(handle->SSIZ)
#define DCHDSIZ *(handle->DSIZ)
#define DCHCSIZ *(handle->CSIZ)
#define DCHSPTR *(handle->SPTR)
#define DCHDPTR *(handle->DPTR)
#define DCHCPTR *(handle->CPTR)
#define DCHDAT *(handle->DAT)

typedef union {
    struct {
      uint32_t CHPRI:2;
      uint32_t CHEDET:1;
      uint32_t :1;
      uint32_t CHAEN:1;
      uint32_t CHCHN:1;
      uint32_t CHAED:1;
      uint32_t CHEN:1;
      uint32_t CHCHNS:1;
      uint32_t :2;
      uint32_t CHPATLEN:1;
      uint32_t :1;
      uint32_t CHPIGNEN:1;
      uint32_t :1;
      uint32_t CHBUSY:1;
      uint32_t :8;
      uint32_t CHPIGN:8;
    };
    struct {
      uint32_t w:32;
    };
} DCHxCON_t;

typedef union {
    struct {
      uint32_t :3;
      uint32_t AIRQEN:1;
      uint32_t SIRQEN:1;
      uint32_t PATEN:1;
      uint32_t CABORT:1;
      uint32_t CFORCE:1;
      uint32_t CHSIRQ:8;
      uint32_t CHAIRQ:8;
    };
    struct {
      uint32_t w:32;
    };
} DCHxECON_t;

typedef union {
    struct {
      uint32_t CHERIF:1;
      uint32_t CHTAIF:1;
      uint32_t CHCCIF:1;
      uint32_t CHBCIF:1;
      uint32_t CHDHIF:1;
      uint32_t CHDDIF:1;
      uint32_t CHSHIF:1;
      uint32_t CHSDIF:1;
      uint32_t :8;
      uint32_t CHERIE:1;
      uint32_t CHTAIE:1;
      uint32_t CHCCIE:1;
      uint32_t CHBCIE:1;
      uint32_t CHDHIE:1;
      uint32_t CHDDIE:1;
      uint32_t CHSHIE:1;
      uint32_t CHSDIE:1;
    };
    struct {
      uint32_t w:32;
    };
} DCHxINT_t;

struct __DMA_Descriptor__{
    volatile DCHxCON_t  *   CON;
    volatile DCHxECON_t *   ECON;
    volatile uint32_t   *   ECONSET;
    volatile DCHxINT_t  *   INT;
    volatile uint32_t   *   INTCLR;
    
    volatile uint32_t   *   SSA;
    volatile uint32_t   *   DSA;
    
    volatile uint32_t   *   SSIZ;
    volatile uint32_t   *   DSIZ;
    volatile uint32_t   *   CSIZ;
    
    volatile uint32_t   *   SPTR;
    volatile uint32_t   *   DPTR;
    volatile uint32_t   *   CPTR;
    
    volatile uint32_t   *   DAT;
    
    volatile uint32_t   *   IECREG;
    
    uint32_t                moduleID;
    uint32_t                iecMask;
};

#if defined(DCH7CON)
#define DMA_CHANNELCOUNT 8
#elif defined(DCH6CON)
#define DMA_CHANNELCOUNT 7
#elif defined(DCH5CON)
#define DMA_CHANNELCOUNT 6
#elif defined(DCH4CON)
#define DMA_CHANNELCOUNT 5
#elif defined(DCH3CON)
#define DMA_CHANNELCOUNT 4
#elif defined(DCH2CON)
#define DMA_CHANNELCOUNT 3
#elif defined(DCH1CON)
#define DMA_CHANNELCOUNT 2
#elif defined(DCH0CON)
#define DMA_CHANNELCOUNT 1
#else
#error No DMA Channels available on this device!
#endif

extern uint32_t DMA_available[];

#endif