#ifndef DMAUTIL_INC
#define DMAUTIL_INC

#include <xc.h>
#include <stdint.h>
#include <string.h>
#include <sys/kmem.h>

#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "DMA.h"

#define RINGBUFFER_DIRECTION_RX 0
#define RINGBUFFER_DIRECTION_TX 1

typedef struct __DMA_RingBuffer_Descriptor__ DMA_RINGBUFFERHANDLE_t;

DMA_RINGBUFFERHANDLE_t * DMA_createRingBuffer(uint32_t bufferSize, uint32_t dataSize, uint32_t * dataSrc, uint32_t dataReadyInt, uint32_t prio, uint32_t direction);
void DMA_freeRingBuffer(DMA_RINGBUFFERHANDLE_t * handle);

uint32_t DMA_RB_available(DMA_RINGBUFFERHANDLE_t * handle);
uint32_t DMA_RB_read(DMA_RINGBUFFERHANDLE_t * handle, uint32_t * dst, uint32_t size);
uint32_t DMA_RB_readSB(DMA_RINGBUFFERHANDLE_t * handle, StreamBufferHandle_t buffer, uint32_t size);
uint32_t DMA_RB_flush(DMA_RINGBUFFERHANDLE_t * handle);

struct __DMA_RingBuffer_Descriptor__{
    DMA_HANDLE_t * channelHandle;
    
    uint32_t direction;
    uint32_t lastReadPos;
    uint32_t bufferSize;
    uint32_t dataSize;
    
    uint32_t * data;
};

#endif