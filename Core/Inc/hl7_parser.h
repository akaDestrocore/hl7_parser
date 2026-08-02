#ifndef HL7_PARSER_H
#define HL7_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include "patient_vitals.h"

#define HL7_CONTROL_ID_LEN 32u

// ASSUMPTION (per project spec 3.2): encoding characters in MSH-2 are always
// exactly "^~\&". We do not read them from the wire; a message using different
// delimiters will parse incorrectly and is out of scope for this version.

typedef struct Hl7Parser_Result_t
{
    bool success;        // true -> caller should ACK with AA, false -> AE
    bool haveControlId;   // true if MSH-10 was extracted, even if success == false
    char controlId[HL7_CONTROL_ID_LEN];
} Hl7Parser_Result_t;

/**
  * @brief Parses one complete HL7 message (MSH/PID/OBX only) extracted by the MLLP framer
  * @param pMsgBuffer Pointer to the raw message bytes (not null-terminated)
  * @param msgLen Length of the message in bytes
  * @param pVitals Vitals record to populate; known measurement fields persist if absent
  *        from this message, unknown-OBX slots are cleared and repopulated each call
  * @param pResult Output: ACK disposition (AA/AE) and message control ID if found
  * @retval None
  */
void hl7Parser_parseMessage(const uint8_t *pMsgBuffer, uint16_t msgLen,
                             PatientVitals_t *pVitals, Hl7Parser_Result_t *pResult);

#endif // HL7_PARSER_H