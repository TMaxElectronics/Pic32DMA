#ifndef DMAUTIL_INC
#define DMAUTIL_INC

#include <xc.h>
#include <stdint.h>
#include <string.h>
#include <sys/kmem.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "DMA.h"

#define RINGBUFFER_DIRECTION_RX 0
#define RINGBUFFER_DIRECTION_TX 1

typedef struct __DMA_RingBuffer_Descriptor__ DMA_RingBufferHandle_t;

DMA_RingBufferHandle_t * DMA_createRingBuffer(uint32_t bufferSize, uint32_t dataSize, uint32_t * dataSrc, uint32_t dataReadyInt, uint32_t prio, uint32_t direction);
void DMA_freeRingBuffer(DMA_RingBufferHandle_t * handle);

uint32_t DMA_RB_available(DMA_RingBufferHandle_t * handle);
uint32_t DMA_RB_availableWords(DMA_RingBufferHandle_t * handle);
uint32_t DMA_RB_write(DMA_RingBufferHandle_t * handle, uint8_t * src, uint32_t size);
uint32_t DMA_RB_read(DMA_RingBufferHandle_t * handle, uint8_t * dst, uint32_t size);
uint32_t DMA_RB_readWords(DMA_RingBufferHandle_t * handle, uint8_t * dst, uint32_t size);
uint32_t DMA_RB_readWordPtr(DMA_RingBufferHandle_t * handle, void ** dst);
uint32_t DMA_RB_readSB(DMA_RingBufferHandle_t * handle, StreamBufferHandle_t buffer, uint32_t size);
uint32_t DMA_RB_flush(DMA_RingBufferHandle_t * handle);
uint32_t DMA_RB_waitForData(DMA_RingBufferHandle_t * handle, uint32_t timeout);

struct __DMA_RingBuffer_Descriptor__{
    DmaHandle_t * channelHandle;
    
    uint32_t direction;
    uint32_t lastReadPos;
    uint32_t bufferSize;
    uint32_t dataSize;
    uint32_t dataReadyInt;
    uint32_t restartOnError;
    
    uint8_t * data;
    
    SemaphoreHandle_t dataSemaphore;
};

#endif