#include <xc.h>
#include <stdint.h>
#include <string.h>
#include <sys/kmem.h>

#include "DMA.h"
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "DMAutils.h"
#include "DMAconfig.h"
#include "System.h"

DMA_RINGBUFFERHANDLE_t * DMA_createRingBuffer(uint32_t bufferSize, uint32_t dataSize, uint32_t * dataSrc, uint32_t dataReadyInt, uint32_t prio, uint32_t direction){
    DMA_RINGBUFFERHANDLE_t * ret = pvPortMalloc(sizeof(DMA_RINGBUFFERHANDLE_t));
    
    ret->direction = direction;
    ret->lastReadPos = 0;
    ret->bufferSize = bufferSize;
    ret->dataSize = dataSize;
    
    ret->channelHandle = DMA_allocateChannel();
    
    if(ret->channelHandle == NULL){
        vPortFree(ret);
        return NULL;
    }
        
    DMA_setChannelAttributes(ret->channelHandle, 0, 0, 0, 1, prio);
    DMA_setInterruptConfig(ret->channelHandle, 0,0,0,0,0,0,0,0);
    DMA_setTransferAttributes(ret->channelHandle, dataSize, dataReadyInt, -1);
    
    ret->data = SYS_makeCoherent(pvPortMalloc(bufferSize));
    
    if(direction == RINGBUFFER_DIRECTION_RX){
    
        DMA_setSrcConfig(ret->channelHandle, dataSrc, dataSize);
        DMA_setDestConfig(ret->channelHandle, ret->data, bufferSize);
        
    }else if(direction == RINGBUFFER_DIRECTION_TX){
        
        DMA_setDestConfig(ret->channelHandle, dataSrc, dataSize);
        DMA_setSrcConfig(ret->channelHandle, ret->data, bufferSize);
        
    }else{
        DMA_freeChannel(ret->channelHandle);
        vPortFree(ret);
        return NULL;
    }
    
    //and finally enable the DMA channel
    DMA_setEnabled(ret->channelHandle, 1);
    
    return ret;
}

void DMA_freeRingBuffer(DMA_RINGBUFFERHANDLE_t * handle){
    if(handle == NULL) return;
    
    DMA_freeChannel(handle->channelHandle);
    
    vPortFree(SYS_makeNonCoherent(handle->data));
    vPortFree(handle);
}

#pragma GCC push_options
#pragma GCC optimize ("Os")

//returns either the amount of data available for reading out of amount of data available for the dma to write to the target
uint32_t DMA_RB_available(DMA_RINGBUFFERHANDLE_t * handle){
    if(handle->direction == RINGBUFFER_DIRECTION_RX){
        if(*(handle->channelHandle->DPTR) >= handle->lastReadPos){
            return *(handle->channelHandle->DPTR) - handle->lastReadPos;
        }else{
            return *(handle->channelHandle->DPTR) + handle->bufferSize - handle->lastReadPos;
        }
    }else{
        if(*(handle->channelHandle->DPTR) <= handle->lastReadPos){
            return handle->lastReadPos - *(handle->channelHandle->DPTR);
        }else{
            return handle->lastReadPos + handle->bufferSize - *(handle->channelHandle->DPTR);
        }
    }
}

uint32_t DMA_RB_read(DMA_RINGBUFFERHANDLE_t * handle, uint8_t * dst, uint32_t size){
    if(handle->direction != RINGBUFFER_DIRECTION_RX) return 0;
    
    uint32_t available = DMA_RB_available(handle);
    if(size > available) size = available;
    
    uint32_t currPos = 0;
    while((handle->lastReadPos != *(handle->channelHandle->DPTR)) && (currPos != size)){
        dst[currPos++] = handle->data[handle->lastReadPos++];
        if(handle->lastReadPos >= handle->bufferSize) handle->lastReadPos = 0;
    }
    
    return size;
}

#pragma GCC pop_options

uint32_t DMA_RB_write(DMA_RINGBUFFERHANDLE_t * handle, uint8_t * src, uint32_t size){
    if(handle->direction != RINGBUFFER_DIRECTION_TX) return 0;
    uint32_t available = handle->bufferSize - 1 - DMA_RB_available(handle);
    if(size > available) size = available;
    
    uint32_t currPos = 0;
    while((handle->lastReadPos != *(handle->channelHandle->DPTR)) && (currPos != size)){
        handle->data[handle->lastReadPos++] = src[currPos++];
        if(handle->lastReadPos >= handle->bufferSize) handle->lastReadPos = 0;
    }
    
    return size;
}

uint32_t DMA_RB_readSB(DMA_RINGBUFFERHANDLE_t * handle, StreamBufferHandle_t buffer, uint32_t size){
    if(handle->direction != RINGBUFFER_DIRECTION_RX) return 0;
    
    uint32_t available = DMA_RB_available(handle);
    if(size > available) size = available;
    uint32_t freeSpace = xStreamBufferSpacesAvailable(buffer);
    if(size > freeSpace) size = freeSpace;
    
    uint8_t * dataStartAddr = (uint8_t *) ((uint32_t) handle->data + handle->lastReadPos);
    
    if(size == 0) return 0;
    
    //let the xStreamBufferSend routine copy all the data itself, but make sure we take a buffer wraparound into account
    uint32_t bytesWritten = 0;
    if(*(handle->channelHandle->DPTR) >= handle->lastReadPos){
        bytesWritten = xStreamBufferSend(buffer, dataStartAddr, size, 0);
        handle->lastReadPos += bytesWritten;
        if(handle->lastReadPos >= handle->bufferSize) handle->lastReadPos = 0;
    }else{
        //start with the date between current read pointer and buffer end
        uint32_t bytesRemaining = size;
        bytesWritten = xStreamBufferSend(buffer, dataStartAddr, handle->bufferSize - handle->lastReadPos, 0);
        handle->lastReadPos += bytesWritten;
        size -= bytesWritten;
        
        //did we read everything? should return normally due to space check above, but make sure just incase
        if(handle->lastReadPos != handle->bufferSize){
            return bytesWritten;
        }
        
        //copy the remaining part of the data
        handle->lastReadPos = 0;
        bytesWritten += xStreamBufferSend(buffer, dataStartAddr, bytesRemaining, 0);
    }
    
    return bytesWritten;
}

uint32_t DMA_RB_flush(DMA_RINGBUFFERHANDLE_t * handle){
    uint32_t reEnable = 0;
    if(DMA_isEnabled(handle->channelHandle)) reEnable = 1;
    
    DMA_abortTransfer(handle->channelHandle);
    handle->lastReadPos = 0;
    
    if(reEnable) DMA_setEnabled(handle->channelHandle, 1);
}