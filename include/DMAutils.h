#ifndef DMAUTIL_INC
#define DMAUTIL_INC

#include <xc.h>
#include <stdint.h>
#include <string.h>
#include <sys/kmem.h>
#include "DMA.h"

typedef struct __DMA_RingBuffer_Descriptor__ DMA_RINGBUFFERHANDLE_t;

DMA_RINGBUFFERHANDLE_t * DMA_createRingBuffer(uint32_t bufferSize, uint32_t dataSize, uint32_t * dataSrc, uint32_t dataReadyInt, uint32_t prio);
void DMA_freeRingBuffer(DMA_RINGBUFFERHANDLE_t * handle);

uint32_t DMA_RB_available(DMA_RINGBUFFERHANDLE_t * handle);
uint32_t DMA_RB_read(DMA_RINGBUFFERHANDLE_t * handle, uint32_t * dst, uint32_t size);
uint32_t DMA_RB_readSB(DMA_RINGBUFFERHANDLE_t * handle, StreamBufferHandle_t buffer, uint32_t size);
uint32_t DMA_RB_flush(DMA_RINGBUFFERHANDLE_t * handle);

struct __DMA_RingBuffer_Descriptor__{
    DMA_HANDLE_t * channelHandle;
    
    uint32_t lastReadPos;
    uint32_t bufferSize;
    uint32_t dataSize;
    
    uint32_t * data;
};

#endif