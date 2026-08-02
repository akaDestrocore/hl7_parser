#include "hl7_parser.h"
#include <string.h>

#define HL7_MAX_FIELDS       16u
#define HL7_MAX_SUBFIELDS     4u
#define HL7_SEG_TAG_LEN       3u

typedef struct Hl7Parser_FieldRef_t
{
    uint16_t offset;  // offset into the segment buffer
    uint16_t length;  // length of this field, excluding the separator
} Hl7Parser_FieldRef_t;

typedef enum Measurement_e
{
    Measurement_HEART_RATE = 0,
    Measurement_SPO2,
    Measurement_TEMPERATURE,
    Measurement_NIBP,
    Measurement_UNKNOWN
} Measurement_e;

typedef struct MeasurementLookup_t
{
    const char *pCode;
    Measurement_e measurement;
} MeasurementLookup_t;


static const MeasurementLookup_t g_measurementTable[] =
{
    { "8867-4",  Measurement_HEART_RATE },
    { "59408-5", Measurement_SPO2 },
    { "8310-5",  Measurement_TEMPERATURE },
    { "55284-4", Measurement_NIBP }
};

#define MEASUREMENT_TABLE_SIZE (sizeof(g_measurementTable) / sizeof(g_measurementTable[0]))

static uint16_t hl7Parser_findByte(const uint8_t *pData, uint16_t len, uint8_t target);
static uint8_t hl7Parser_splitFields(const uint8_t *pSegData, uint16_t segLen, uint8_t sep,
                                      Hl7Parser_FieldRef_t *pFields, uint8_t maxFields);
static void hl7Parser_copyField(const uint8_t *pSegData, const Hl7Parser_FieldRef_t *pField,
                                 char *pDest, size_t destSize);
static void hl7Parser_processMshSegment(const uint8_t *pSegData, uint16_t segLen, Hl7Parser_Result_t *pResult);
static void hl7Parser_processPidSegment(const uint8_t *pSegData, uint16_t segLen, PatientVitals_t *pVitals);
static void hl7Parser_processObxSegment(const uint8_t *pSegData, uint16_t segLen, PatientVitals_t *pVitals);
static void hl7Parser_storeMeasurement(PatientVitals_t *pVitals, const char *pCode, const char *pValue);
static void hl7Parser_storeUnknownObx(PatientVitals_t *pVitals, const char *pCode, const char *pValue);

/**
  * @brief Finds the first occurrence of a byte within a bounded region
  * @param pData Pointer to the search region
  * @param len Length of the search region
  * @param target Byte value to find
  * @retval Index of the first match, or len if not found
  */
static uint16_t hl7Parser_findByte(const uint8_t *pData, uint16_t len, uint8_t target)
{
    uint16_t i;

    for (i = 0u; i < len; i++)
    {
        if (target == pData[i])  // Yoda condition
        {
            break;
        }
    }

    return i;
}

/**
  * @brief Splits a bounded byte region into fields on a separator byte, without mutating the source
  * @param pSegData Pointer to the region to split
  * @param segLen Length of the region
  * @param sep Separator byte
  * @param pFields Output array of field references (offset/length pairs, relative to pSegData)
  * @param maxFields Capacity of pFields
  * @retval Number of fields written to pFields
  */
static uint8_t hl7Parser_splitFields(const uint8_t *pSegData, uint16_t segLen, uint8_t sep,
                                      Hl7Parser_FieldRef_t *pFields, uint8_t maxFields)
{
    uint8_t fieldCount = 0u;
    uint16_t pos = 0u;

    while ((pos <= segLen) && (fieldCount < maxFields))
    {
        uint16_t remaining = (uint16_t)(segLen - pos);
        uint16_t relEnd = hl7Parser_findByte(&pSegData[pos], remaining, sep);

        pFields[fieldCount].offset = pos;
        pFields[fieldCount].length = relEnd;
        fieldCount++;

        if (relEnd == remaining)
        {
            break;  // no more separators - that was the last field
        }

        pos = (uint16_t)(pos + relEnd + 1u);
    }

    return fieldCount;
}

/**
  * @brief Copies one field into a null-terminated destination buffer, truncating if needed
  * @param pSegData Pointer to the segment the field reference is relative to
  * @param pField Field reference (offset/length) to copy
  * @param pDest Destination buffer
  * @param destSize Size of the destination buffer, including space for the null terminator
  * @retval None
  */
static void hl7Parser_copyField(const uint8_t *pSegData, const Hl7Parser_FieldRef_t *pField,
                                 char *pDest, size_t destSize)
{
    uint16_t copyLen = pField->length;

    if (copyLen >= destSize)
    {
        copyLen = (uint16_t)(destSize - 1u);
    }

    (void)memcpy(pDest, &pSegData[pField->offset], copyLen);
    pDest[copyLen] = '\0';
}

/**
  * @brief Extracts MSH-10 (message control ID) from an MSH segment
  * @param pSegData Pointer to the MSH segment bytes
  * @param segLen Length of the segment
  * @param pResult Result struct to populate with the control ID
  * @retval None
  */
static void hl7Parser_processMshSegment(const uint8_t *pSegData, uint16_t segLen, Hl7Parser_Result_t *pResult)
{
    Hl7Parser_FieldRef_t fields[HL7_MAX_FIELDS];
    uint8_t fieldCount = hl7Parser_splitFields(pSegData, segLen, (uint8_t)'|', fields, HL7_MAX_FIELDS);

    // MSH numbering quirk: the field separator itself is MSH-1 and is consumed as the
    // split delimiter rather than emitted as a token, so naive pipe-splitting yields
    // fields[i] == MSH-(i+1) for i >= 1. MSH-10 is therefore fields[9].
    const uint8_t MSH10_FIELD_INDEX = 9u;

    if (fieldCount > MSH10_FIELD_INDEX)
    {
        hl7Parser_copyField(pSegData, &fields[MSH10_FIELD_INDEX], pResult->controlId, sizeof(pResult->controlId));
        pResult->haveControlId = true;
    }
}

/**
  * @brief Extracts PID-3 (patient identifier) from a PID segment
  * @param pSegData Pointer to the PID segment bytes
  * @param segLen Length of the segment
  * @param pVitals Vitals record to populate with the patient ID
  * @retval None
  */
static void hl7Parser_processPidSegment(const uint8_t *pSegData, uint16_t segLen, PatientVitals_t *pVitals)
{
    Hl7Parser_FieldRef_t fields[HL7_MAX_FIELDS];
    uint8_t fieldCount = hl7Parser_splitFields(pSegData, segLen, (uint8_t)'|', fields, HL7_MAX_FIELDS);
    const uint8_t PID3_FIELD_INDEX = 3u;

    if (fieldCount > PID3_FIELD_INDEX)
    {
        hl7Parser_copyField(pSegData, &fields[PID3_FIELD_INDEX], pVitals->patientId, sizeof(pVitals->patientId));
        pVitals->patientIdValid = true;
    }
}

/**
  * @brief Extracts OBX-3 (code) and OBX-5 (value) from an OBX segment and stores the result
  * @param pSegData Pointer to the OBX segment bytes
  * @param segLen Length of the segment
  * @param pVitals Vitals record to update
  * @retval None
  */
static void hl7Parser_processObxSegment(const uint8_t *pSegData, uint16_t segLen, PatientVitals_t *pVitals)
{
    Hl7Parser_FieldRef_t fields[HL7_MAX_FIELDS];
    uint8_t fieldCount = hl7Parser_splitFields(pSegData, segLen, (uint8_t)'|', fields, HL7_MAX_FIELDS);
    const uint8_t OBX3_FIELD_INDEX = 3u;  // observation identifier: <code>^<text>
    const uint8_t OBX5_FIELD_INDEX = 5u;  // observation value

    // NOTE: we deliberately don't branch on OBX-2 (value type NM/ST) - both a plain
    // number and a "120/80"-style string copy cleanly as display text, so a single
    // code path handles all four known measurement types plus unknowns.

    if ((fieldCount > OBX5_FIELD_INDEX) && (fieldCount > OBX3_FIELD_INDEX))
    {
        Hl7Parser_FieldRef_t codeSubFields[HL7_MAX_SUBFIELDS];
        char code[OBX_CODE_LEN];
        char value[MEAS_VALUE_LEN];
        uint8_t codeSubCount;

        codeSubCount = hl7Parser_splitFields(&pSegData[fields[OBX3_FIELD_INDEX].offset],
                                              fields[OBX3_FIELD_INDEX].length,
                                              (uint8_t)'^', codeSubFields, HL7_MAX_SUBFIELDS);

        if (codeSubCount > 0u)
        {
            hl7Parser_copyField(&pSegData[fields[OBX3_FIELD_INDEX].offset], &codeSubFields[0], code, sizeof(code));
        }
        else
        {
            code[0] = '\0';
        }

        hl7Parser_copyField(pSegData, &fields[OBX5_FIELD_INDEX], value, sizeof(value));
        hl7Parser_storeMeasurement(pVitals, code, value);
    }
    // else: malformed OBX (too few fields) - skip this observation, don't fail the message
}

/**
  * @brief Looks up an OBX-3 code against the known measurement table and stores the value
  * @param pVitals Vitals record to update
  * @param pCode Null-terminated OBX-3 code
  * @param pValue Null-terminated OBX-5 value
  * @retval None
  */
static void hl7Parser_storeMeasurement(PatientVitals_t *pVitals, const char *pCode, const char *pValue)
{
    Measurement_e measurement = Measurement_UNKNOWN;
    uint8_t i;

    for (i = 0u; i < (uint8_t)MEASUREMENT_TABLE_SIZE; i++)
    {
        if (0 == strcmp(pCode, g_measurementTable[i].pCode))  // Yoda condition
        {
            measurement = g_measurementTable[i].measurement;
            break;
        }
    }

    switch (measurement)
    {
        case Measurement_HEART_RATE:
            (void)strncpy(pVitals->heartRateValue, pValue, sizeof(pVitals->heartRateValue) - 1u);
            pVitals->heartRateValue[sizeof(pVitals->heartRateValue) - 1u] = '\0';
            pVitals->heartRateValid = true;
            break;

        case Measurement_SPO2:
            (void)strncpy(pVitals->spo2Value, pValue, sizeof(pVitals->spo2Value) - 1u);
            pVitals->spo2Value[sizeof(pVitals->spo2Value) - 1u] = '\0';
            pVitals->spo2Valid = true;
            break;

        case Measurement_TEMPERATURE:
            (void)strncpy(pVitals->temperatureValue, pValue, sizeof(pVitals->temperatureValue) - 1u);
            pVitals->temperatureValue[sizeof(pVitals->temperatureValue) - 1u] = '\0';
            pVitals->temperatureValid = true;
            break;

        case Measurement_NIBP:
            (void)strncpy(pVitals->nibpValue, pValue, sizeof(pVitals->nibpValue) - 1u);
            pVitals->nibpValue[sizeof(pVitals->nibpValue) - 1u] = '\0';
            pVitals->nibpValid = true;
            break;

        case Measurement_UNKNOWN:
        default:
            hl7Parser_storeUnknownObx(pVitals, pCode, pValue);
            break;
    }
}

/**
  * @brief Stores an unrecognized OBX code/value pair in the next free unknown-OBX slot
  * @param pVitals Vitals record to update
  * @param pCode Null-terminated OBX-3 code
  * @param pValue Null-terminated OBX-5 value
  * @retval None
  */
static void hl7Parser_storeUnknownObx(PatientVitals_t *pVitals, const char *pCode, const char *pValue)
{
    if (pVitals->unknownObxCount < MAX_UNKNOWN_OBX)
    {
        uint8_t idx = pVitals->unknownObxCount;

        (void)strncpy(pVitals->unknownObx[idx].code, pCode, sizeof(pVitals->unknownObx[idx].code) - 1u);
        pVitals->unknownObx[idx].code[sizeof(pVitals->unknownObx[idx].code) - 1u] = '\0';

        (void)strncpy(pVitals->unknownObx[idx].value, pValue, sizeof(pVitals->unknownObx[idx].value) - 1u);
        pVitals->unknownObx[idx].value[sizeof(pVitals->unknownObx[idx].value) - 1u] = '\0';

        pVitals->unknownObx[idx].present = true;
        pVitals->unknownObxCount++;
    }
    // else: more than MAX_UNKNOWN_OBX unrecognized codes in one message - extras dropped
}

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
                             PatientVitals_t *pVitals, Hl7Parser_Result_t *pResult)
{
    bool haveMsh = false;
    bool havePid = false;
    uint16_t segStart = 0u;

    pResult->success = false;
    pResult->haveControlId = false;
    pResult->controlId[0] = '\0';
    pVitals->unknownObxCount = 0u;

    while (segStart < msgLen)
    {
        uint16_t remaining = (uint16_t)(msgLen - segStart);
        uint16_t segRelEnd = hl7Parser_findByte(&pMsgBuffer[segStart], remaining, (uint8_t)'\r');
        uint16_t segLen = segRelEnd;

        if (segLen >= HL7_SEG_TAG_LEN)
        {
            const uint8_t *pSeg = &pMsgBuffer[segStart];

            if (0 == memcmp(pSeg, "MSH", HL7_SEG_TAG_LEN))  // Yoda condition
            {
                haveMsh = true;
                hl7Parser_processMshSegment(pSeg, segLen, pResult);
            }
            else if (0 == memcmp(pSeg, "PID", HL7_SEG_TAG_LEN))
            {
                havePid = true;
                hl7Parser_processPidSegment(pSeg, segLen, pVitals);
            }
            else if (0 == memcmp(pSeg, "OBX", HL7_SEG_TAG_LEN))
            {
                hl7Parser_processObxSegment(pSeg, segLen, pVitals);
            }
            else
            {
                // unrecognized segment type (EVN, PV1, ...) - skip per project spec section 2
            }
        }

        segStart = (uint16_t)(segStart + segRelEnd + 1u);
    }

    // ASSUMPTION: both MSH and PID are treated as required segments for an AA response;
    // a message missing either is a parse failure (AE), even if some OBX data was present.
    pResult->success = (haveMsh && havePid);
}