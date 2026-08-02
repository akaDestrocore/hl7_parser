// mllp_framer.c
#include "mllp_framer.h"
#include <string.h>

/**
 * @brief Resets the framer to idle state, discarding any in-progress frame
 * @param pHandle Pointer to the framer instance
 * @retval None
 */
static void mllpFramer_reset(MllpFramer_Handle_t *pHandle)
{
    pHandle->state = MllpFramer_STATE_IDLE;
    pHandle->msgLen = 0u;
}

/**
 * @brief Initializes the MLLP framer to a clean idle state
 * @param pHandle Pointer to the framer instance
 * @retval None
 */
void mllpFramer_init(MllpFramer_Handle_t *pHandle)
{
    mllpFramer_reset(pHandle);
    pHandle->frameStartTick = 0u;
    pHandle->lastFrameHadError = false;
}

/**
 * @brief Feeds a single received byte through the MLLP framing state machine
 * @param pHandle Pointer to the framer instance
 * @param byte Byte pulled from the UART ring buffer
 * @param currentTick Current system tick in ms, stamped when a frame opens
 * @retval true if this byte completed a valid frame (msgBuffer/msgLen now ready), false otherwise
 */
bool mllpFramer_processByte(MllpFramer_Handle_t *pHandle, uint8_t byte, uint32_t currentTick)
{
    switch (pHandle->state)
    {
        case MllpFramer_STATE_IDLE:
            if (MLLP_VT == byte)  // Yoda condition
            {
                pHandle->msgLen = 0u;
                pHandle->state = MllpFramer_STATE_IN_FRAME;
                pHandle->lastFrameHadError = false;
                pHandle->frameStartTick = currentTick;
            }
            break;

        case MllpFramer_STATE_IN_FRAME:
            if (MLLP_VT == byte)
            {
                pHandle->msgLen = 0u;
                pHandle->lastFrameHadError = true;
                pHandle->frameStartTick = currentTick;  // restart timeout window for the new frame
            }
            else if (MLLP_FS == byte)
            {
                pHandle->state = MllpFramer_STATE_GOT_FS;
            }
            else
            {
                if (pHandle->msgLen < MSG_BUFFER_SIZE)
                {
                    pHandle->msgBuffer[pHandle->msgLen] = byte;
                    pHandle->msgLen++;
                }
                else
                {
                    pHandle->lastFrameHadError = true;
                }
            }
            break;

        case MllpFramer_STATE_GOT_FS:
            if (MLLP_CR == byte)
            {
                pHandle->state = MllpFramer_STATE_IDLE;
                return true;
            }
            else
            {
                mllpFramer_reset(pHandle);
                pHandle->lastFrameHadError = true;
            }
            break;

        default:
            mllpFramer_reset(pHandle);
            break;
    }

    return false;
}

/**
 * @brief Polls the framer for a stalled/truncated frame and resets it on timeout
 * @param pHandle Pointer to the framer instance
 * @param currentTick Current system tick count in milliseconds
 * @retval None
 */
void mllpFramer_poll(MllpFramer_Handle_t *pHandle, uint32_t currentTick)
{
    if (MllpFramer_STATE_IDLE != pHandle->state)
    {
        if ((currentTick - pHandle->frameStartTick) > FRAME_TIMEOUT_MS)
        {
            mllpFramer_reset(pHandle);
            pHandle->lastFrameHadError = true;
        }
    }
}