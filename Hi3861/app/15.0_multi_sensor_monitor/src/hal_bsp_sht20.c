#include "hal_bsp_sht20.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_i2c.h"
#include "wifiiot_i2c_ex.h"

#define SHT20_NO_HOLD_TEMP_REG_ADDR  0xF3
#define SHT20_NO_HOLD_HUMI_REG_ADDR  0xF5
#define SHT20_SOFT_RESET_REG_ADDR    0xFE

static uint32_t SHT20_RecvData(uint8_t *data, size_t size)
{
    WifiIotI2cData i2cData = {0};

    i2cData.receiveBuf = data;
    i2cData.receiveLen = size;
    return I2cRead(SHT20_I2C_IDX, SHT20_I2C_ADDR, &i2cData);
}

static uint32_t SHT20_WriteByteData(uint8_t byte)
{
    uint8_t buffer[] = {byte};
    WifiIotI2cData i2cData = {0};

    i2cData.sendBuf = buffer;
    i2cData.sendLen = sizeof(buffer);
    return I2cWrite(SHT20_I2C_IDX, SHT20_I2C_ADDR, &i2cData);
}

uint32_t SHT20_ReadData(float *temp, float *humi)
{
    uint32_t result;
    uint16_t rawData;
    uint8_t buffer[3] = {0};

    result = SHT20_WriteByteData(SHT20_NO_HOLD_TEMP_REG_ADDR);
    if (result != 0) {
        return result;
    }
    usleep(85 * 1000);

    result = SHT20_RecvData(buffer, sizeof(buffer));
    if (result != 0) {
        return result;
    }
    rawData = ((uint16_t)buffer[0] << 8) | buffer[1];
    rawData &= 0xFFFC;
    *temp = 175.72f * rawData / 65536.0f - 46.85f;

    memset(buffer, 0, sizeof(buffer));
    result = SHT20_WriteByteData(SHT20_NO_HOLD_HUMI_REG_ADDR);
    if (result != 0) {
        return result;
    }
    usleep(50 * 1000);

    result = SHT20_RecvData(buffer, sizeof(buffer));
    if (result != 0) {
        return result;
    }
    rawData = ((uint16_t)buffer[0] << 8) | buffer[1];
    rawData &= 0xFFFC;
    *humi = 125.0f * rawData / 65536.0f - 6.0f;
    return 0;
}

uint32_t SHT20_Init(void)
{
    uint32_t result;

    GpioInit();
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_10,
              WIFI_IOT_IO_FUNC_GPIO_10_I2C0_SDA);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_9,
              WIFI_IOT_IO_FUNC_GPIO_9_I2C0_SCL);

    I2cInit(WIFI_IOT_I2C_IDX_0, 400000);
    result = I2cSetBaudrate(WIFI_IOT_I2C_IDX_0, 400000);
    if (result != 0) {
        return result;
    }

    result = SHT20_WriteByteData(SHT20_SOFT_RESET_REG_ADDR);
    if (result != 0) {
        return result;
    }
    usleep(100 * 1000);
    return 0;
}
