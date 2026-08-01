// ring_buffer.c
#include "ring_buffer.h"

/**
 * @brief Initializes a ring buffer to an empty state
 * @param pHandle Pointer to the ring buffer instance
 * @retval None
 */
void ringBuffer_init(RingBuffer_Handle_t *pHandle)
{
    pHandle->head = 0u;
    pHandle->tail = 0u;
}

/**
 * @brief Pushes one byte into the ring buffer, intended to be called from the UART RX ISR
 * @param pHandle Pointer to the ring buffer instance
 * @param byte Byte received from UART
 * @retval true if the byte was stored, false if the buffer was full (byte dropped)
 */
bool ringBuffer_push(RingBuffer_Handle_t *pHandle, uint8_t byte)
{
    uint16_t nextHead = (uint16_t)((pHandle->head + 1u) & (RING_BUFFER_SIZE - 1u));

    if (nextHead == pHandle->tail)
    {
        // buffer full - drop the byte, caller can track overrun stats separately
        return false;
    }

    pHandle->buffer[pHandle->head] = byte;
    pHandle->head = nextHead;
    return true;
}

/**
 * @brief Pops one byte from the ring buffer, intended to be called from the main loop
 * @param pHandle Pointer to the ring buffer instance
 * @param pByte Output pointer to receive the popped byte
 * @retval true if a byte was available and popped, false if the buffer was empty
 */
bool ringBuffer_pop(RingBuffer_Handle_t *pHandle, uint8_t *pByte)
{
    if (pHandle->head == pHandle->tail)
    {
        return false;
    }

    *pByte = pHandle->buffer[pHandle->tail];
    pHandle->tail = (uint16_t)((pHandle->tail + 1u) & (RING_BUFFER_SIZE - 1u));
    return true;
}

/**
 * @brief Checks whether the ring buffer currently holds no data
 * @param pHandle Pointer to the ring buffer instance
 * @retval true if empty, false otherwise
 */
bool ringBuffer_isEmpty(const RingBuffer_Handle_t *pHandle)
{
    return (pHandle->head == pHandle->tail);
}