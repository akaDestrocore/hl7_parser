// mllp_framer.h
#ifndef MLLP_FRAMER_H
#define MLLP_FRAMER_H

#include <stdint.h>
#include <stdbool.h>

#define MLLP_VT   0x0Bu
#define MLLP_FS   0x1Cu
#define MLLP_CR   0x0Du

#define MSG_BUFFER_SIZE     1024u  // max HL7 payload we can hold per message
#define FRAME_TIMEOUT_MS    3000u  // discard an open frame if not closed within this window

typedef enum MllpFramer_State_e
{
    MllpFramer_STATE_IDLE = 0,       // waiting for VT
    MllpFramer_STATE_IN_FRAME,       // collecting payload bytes
    MllpFramer_STATE_GOT_FS          // saw FS, waiting for CR to confirm close
} MllpFramer_State_e;

typedef struct MllpFramer_Handle_t
{
    MllpFramer_State_e state;
    uint8_t msgBuffer[MSG_BUFFER_SIZE];
    uint16_t msgLen;
    uint32_t frameStartTick;
    bool lastFrameHadError;   // set on overflow / truncation / second-VT, cleared each new frame
} MllpFramer_Handle_t;

void mllpFramer_init(MllpFramer_Handle_t *pHandle);

// Feed one byte at a time. Returns true when *pHandle->msgBuffer holds a complete,
// validated frame (VT...FS CR stripped). Caller should hand msgBuffer/msgLen to the
// HL7 parser immediately, then the framer resets itself for the next frame.
bool mllpFramer_processByte(MllpFramer_Handle_t *pHandle, uint8_t byte);

// Call once per main loop iteration (independent of new bytes) so a frame that
// opened and then went silent gets timed out instead of blocking forever.
void mllpFramer_poll(MllpFramer_Handle_t *pHandle, uint32_t currentTick);

#endif // MLLP_FRAMER_H