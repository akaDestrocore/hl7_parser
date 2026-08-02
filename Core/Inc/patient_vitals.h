#ifndef PATIENT_VITALS_H
#define PATIENT_VITALS_H

#include <stdint.h>
#include <stdbool.h>

#define PATIENT_ID_LEN     32u
#define MEAS_VALUE_LEN      16u
#define OBX_CODE_LEN        16u
#define MAX_UNKNOWN_OBX      4u  // unrecognized OBX codes are transient (this message only), not persisted like the 4 known types

typedef struct PatientVitals_UnknownObx_t
{
    char code[OBX_CODE_LEN];
    char value[MEAS_VALUE_LEN];
    bool present;
} PatientVitals_UnknownObx_t;

typedef struct PatientVitals_t
{
    char patientId[PATIENT_ID_LEN];
    bool patientIdValid;

    char heartRateValue[MEAS_VALUE_LEN];
    bool heartRateValid;

    char spo2Value[MEAS_VALUE_LEN];
    bool spo2Valid;

    char temperatureValue[MEAS_VALUE_LEN];
    bool temperatureValid;

    char nibpValue[MEAS_VALUE_LEN];
    bool nibpValid;

    PatientVitals_UnknownObx_t unknownObx[MAX_UNKNOWN_OBX];
    uint8_t unknownObxCount;  // reset to 0 at the start of every parsed message
} PatientVitals_t;

/**
  * @brief Resets a patient vitals record to a clean empty state
  * @param pVitals Pointer to the vitals record to initialize
  * @retval None
  */
void patientVitals_init(PatientVitals_t *pVitals);

#endif // PATIENT_VITALS_H