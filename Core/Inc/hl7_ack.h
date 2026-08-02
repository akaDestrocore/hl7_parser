#ifndef HL7_ACK_H
#define HL7_ACK_H

#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "hl7_parser.h"

/**
  * @brief Builds an HL7 ACK (MSH/MSA) for the given parse result, MLLP-wraps it,
  *        and transmits it over the given UART (blocking transmit - message is
  *        small, so this stalls the main loop only briefly and does not affect
  *        the interrupt-driven receive path).
  * @param pHuart UART handle to transmit the ACK on
  * @param pResult Parse result from hl7Parser_parseMessage; success -> AA, failure -> AE.
  *        If pResult->haveControlId is false, no ACK is sent (per project spec 3.3 -
  *        we don't have enough of the original message to ACK against).
  * @retval 0 on success, error code otherwise
  */
uint8_t hl7Ack_send(UART_HandleTypeDef *pHuart, const Hl7Parser_Result_t *pResult);

#endif // HL7_ACK_H