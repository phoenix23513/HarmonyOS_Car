#ifndef FRONT_LIGHT_UART_H
#define FRONT_LIGHT_UART_H

#include <stdint.h>

/** Initialize the Hi3861 UART2 connection to the STM32. */
uint32_t FrontLightUartInit(void);

/** Send one front-light state frame: 0 means off, 1 means on. */
uint32_t FrontLightUartSend(uint8_t lightOn);

#endif
