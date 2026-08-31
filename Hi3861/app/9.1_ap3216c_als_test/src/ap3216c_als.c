#include "ap3216c_als.h"

#include <stddef.h>
#include <unistd.h>

#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_i2c.h"
#include "wifiiot_i2c_ex.h"

/* The Hi3861 I2C API uses the 8-bit AP3216C address: 0x1E << 1. */
#define AP3216C_I2C_ADDRESS 0x3C
#define AP3216C_I2C_INDEX WIFI_IOT_I2C_IDX_0
#define AP3216C_I2C_BAUDRATE 400000

#define AP3216C_SYSTEM_CONFIG_REG 0x00
#define AP3216C_ALS_DATA_LOW_REG 0x0C
#define AP3216C_ALS_DATA_HIGH_REG 0x0D

#define AP3216C_SOFTWARE_RESET 0x04
#define AP3216C_ALS_ACTIVE 0x01

static uint32_t Ap3216cWriteRegister(uint8_t registerAddress, uint8_t value)
{
    uint8_t buffer[2] = {registerAddress, value};
    WifiIotI2cData i2cData = {0};

    i2cData.sendBuf = buffer;
    i2cData.sendLen = sizeof(buffer);
    return I2cWrite(AP3216C_I2C_INDEX, AP3216C_I2C_ADDRESS, &i2cData);
}

static uint32_t Ap3216cReadRegister(uint8_t registerAddress, uint8_t *value)
{
    WifiIotI2cData writeData = {0};
    WifiIotI2cData readData = {0};
    uint32_t result;

    if (value == NULL) {
        return 1;
    }

    writeData.sendBuf = &registerAddress;
    writeData.sendLen = 1;
    result = I2cWrite(AP3216C_I2C_INDEX, AP3216C_I2C_ADDRESS, &writeData);
    if (result != 0) {
        return result;
    }

    readData.receiveBuf = value;
    readData.receiveLen = 1;
    return I2cRead(AP3216C_I2C_INDEX, AP3216C_I2C_ADDRESS, &readData);
}

uint32_t Ap3216cAlsInit(void)
{
    uint32_t result;

    GpioInit();
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_10, WIFI_IOT_IO_FUNC_GPIO_10_I2C0_SDA);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_9, WIFI_IOT_IO_FUNC_GPIO_9_I2C0_SCL);

    result = I2cInit(AP3216C_I2C_INDEX, AP3216C_I2C_BAUDRATE);
    if (result != 0) {
        return result;
    }

    result = I2cSetBaudrate(AP3216C_I2C_INDEX, AP3216C_I2C_BAUDRATE);
    if (result != 0) {
        return result;
    }

    result = Ap3216cWriteRegister(AP3216C_SYSTEM_CONFIG_REG, AP3216C_SOFTWARE_RESET);
    if (result != 0) {
        return result;
    }

    usleep(5000);
    result = Ap3216cWriteRegister(AP3216C_SYSTEM_CONFIG_REG, AP3216C_ALS_ACTIVE);
    if (result != 0) {
        return result;
    }

    /* Allow the first ALS conversion to finish before the task reads it. */
    usleep(100000);
    return 0;
}

uint32_t Ap3216cAlsRead(uint16_t *alsRaw)
{
    uint8_t lowByte = 0;
    uint8_t highByte = 0;
    uint32_t result;

    if (alsRaw == NULL) {
        return 1;
    }

    result = Ap3216cReadRegister(AP3216C_ALS_DATA_LOW_REG, &lowByte);
    if (result != 0) {
        return result;
    }

    result = Ap3216cReadRegister(AP3216C_ALS_DATA_HIGH_REG, &highByte);
    if (result != 0) {
        return result;
    }

    *alsRaw = ((uint16_t)highByte << 8) | lowByte;
    return 0;
}
