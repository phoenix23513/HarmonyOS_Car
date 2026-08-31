#ifndef AP3216C_ALS_H
#define AP3216C_ALS_H

#include <stdint.h>

/**
 * Initialize I2C0 and enable only the AP3216C ambient-light sensor.
 *
 * @return 0 on success, otherwise the Hi3861 I2C error code.
 */
uint32_t Ap3216cAlsInit(void);

/**
 * Read the raw 16-bit ambient-light value from the AP3216C.
 *
 * @param alsRaw Receives the raw ALS register value.
 * @return 0 on success, otherwise the Hi3861 I2C error code.
 */
uint32_t Ap3216cAlsRead(uint16_t *alsRaw);

#endif
