#include "patient_vitals.h"

/**
  * @brief Resets a patient vitals record to a clean empty state
  * @param pVitals Pointer to the vitals record to initialize
  * @retval None
  */
void patientVitals_init(PatientVitals_t *pVitals)
{
    uint8_t i;

    pVitals->patientId[0] = '\0';
    pVitals->patientIdValid = false;

    pVitals->heartRateValue[0] = '\0';
    pVitals->heartRateValid = false;

    pVitals->spo2Value[0] = '\0';
    pVitals->spo2Valid = false;

    pVitals->temperatureValue[0] = '\0';
    pVitals->temperatureValid = false;

    pVitals->nibpValue[0] = '\0';
    pVitals->nibpValid = false;

    pVitals->unknownObxCount = 0u;

    for (i = 0u; i < MAX_UNKNOWN_OBX; i++)
    {
        pVitals->unknownObx[i].code[0] = '\0';
        pVitals->unknownObx[i].value[0] = '\0';
        pVitals->unknownObx[i].present = false;
    }
}