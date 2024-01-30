#include <xc.h>
#include <stdint.h>
#include <string.h>
#include <sys/kmem.h>

#include "DMA.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "DMAutils.h"
#include "DMAconfig.h"
#include "System.h"


static void DMA_RB_ISR(uint32_t evt, void * data);

DMA_RINGBUFFERHANDLE_t * DMA_createRingBuffer(uint32_t bufferSize, uint32_t dataSize, uint32_t * dataSrc, uint32_t dataReadyInt, uint32_t prio, uint32_t direction){
    DMA_RINGBUFFERHANDLE_t * ret = pvPortMalloc(sizeof(DMA_RINGBUFFERHANDLE_t));
    
    ret->direction = direction;
    ret->lastReadPos = 0;
    ret->bufferSize = bufferSize;
    ret->dataSize = dataSize;
    ret->dataReadyInt = dataReadyInt;
    
    ret->channelHandle = DMA_allocateChannel();
    
    ret->dataSemaphore = xSemaphoreCreateBinary();
    
    if(ret->channelHandle == NULL){
        vPortFree(ret);
        return NULL;
    }
        
    DMA_setIRQHandler(ret->channelHandle, DMA_RB_ISR, ret);
    DMA_setChannelAttributes(ret->channelHandle, 0, 0, 0, 1, prio);
    DMA_setInterruptConfig(ret->channelHandle, 0,0,0,0,0,0,1,1);
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
    
    vSemaphoreDelete(handle->dataSemaphore);
    
    vPortFree(SYS_makeNonCoherent(handle->data));
    vPortFree(handle);
}

//general ISR for the ringbuffer, reacts to all interrupts triggered by the channel
void DMA_RB_ISR(uint32_t evt, void * data){
    DMA_RINGBUFFERHANDLE_t * handle = (DMA_RINGBUFFERHANDLE_t *) data;
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    //check which event happened
    if((evt & _DCH0INT_CHTAIF_MASK) || (evt & _DCH0INT_CHERIF_MASK)){
        //transfer aborted or other error => reset data pointers
        handle->lastReadPos = 0;
        
        //now re-enable the channel if desired
        if(handle->restartOnError) DMA_setEnabled(handle->channelHandle, 1);
        
        //now also check if somebody is waiting for data, we should return
        xSemaphoreGiveFromISR(handle->dataSemaphore, &xHigherPriorityTaskWoken);
    }
    
    if(evt & _DCH0INT_CHCCIF_MASK){
        //some DMA event just occurred, tell waiting code that it did so
        xSemaphoreGiveFromISR(handle->dataSemaphore, &xHigherPriorityTaskWoken);
    }

    portEND_SWITCHING_ISR( xHigherPriorityTaskWoken );
}

void DMA_RB_setAbortIRQ(DMA_RINGBUFFERHANDLE_t * handle, uint32_t abortIrq, uint32_t autoRestart){
    handle->restartOnError = autoRestart;
    DMA_setTransferAttributes(handle->channelHandle, handle->dataSize, handle->dataReadyInt, abortIrq);
}

void DMA_RB_setDataSrc(DMA_RINGBUFFERHANDLE_t * handle, void * newDataSrc){
    DMA_setSrcConfig(handle->channelHandle, newDataSrc, handle->bufferSize);
    DMA_RB_flush(handle);
}

//#pragma GCC push_options
//#pragma GCC optimize ("Os")

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
    
    //return however many bytes were read, even if we stopped reading due to a buffer underflow for some reason
    return currPos;
}

//#pragma GCC pop_options

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

uint32_t DMA_RB_waitForData(DMA_RINGBUFFERHANDLE_t * handle, uint32_t timeout){
    //is there already some data in the buffer?
    if(DMA_RB_available(handle) > 0) return 1; //yes, just return
    
    //no, no data in the buffer at the moment, enable irq
    DMA_setInterruptConfig(handle->channelHandle, -1, -1, -1, -1, -1, 1, -1, -1);
    
    //and finally wait for the irq to be called
    
    uint32_t ret = 0;
    
    //first take semaphore
    if(xSemaphoreTake(handle->dataSemaphore, 0)){
        //check if timeout is reasonable (if its too long we might get stuck if isr somehow isn't called) TODO: evaluate and make it reliable
        if(timeout > 10000) timeout = 10000;
        //try to take the semaphore
        ret = xSemaphoreTake(handle->dataSemaphore, timeout);

    }else; //damn we weren't even able to take it here, something was very wrong. Just reset to default state and return
    
    //disable irq
    DMA_setInterruptConfig(handle->channelHandle, -1, -1, -1, -1, -1, 0, -1, -1);

    //free the semaphore again (even if we didn't get it, just to make sure we won't get locked up)
    xSemaphoreGive(handle->dataSemaphore);
        
    return ret;
}