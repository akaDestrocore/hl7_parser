// ring_buffer.h
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define RING_BUFFER_SIZE 512u  // must be power of two for cheap masking

typedef struct RingBuffer_Handle_t
{
    volatile uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;   // written by ISR
    volatile uint16_t tail;   // written by main loop
} RingBuffer_Handle_t;

void ringBuffer_init(RingBuffer_Handle_t *pHandle);
bool ringBuffer_push(RingBuffer_Handle_t *pHandle, uint8_t byte);
bool ringBuffer_pop(RingBuffer_Handle_t *pHandle, uint8_t *pByte);
bool ringBuffer_isEmpty(const RingBuffer_Handle_t *pHandle);

#endif // RING_BUFFER_H