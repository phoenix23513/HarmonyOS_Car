#include "hal_bsp_ap3216c.h"

#include <stddef.h>
#include <unistd.h>

#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_i2c_ex.h"

#define AP3216C_SYSTEM_ADDR  0x00
#define AP3216C_IR_L_ADDR    0x0A
#define AP3216C_IR_H_ADDR    0x0B
#define AP3216C_ALS_L_ADDR   0x0C
#define AP3216C_ALS_H_ADDR   0x0D
#define AP3216C_PS_L_ADDR    0x0E
#define AP3216C_PS_H_ADDR    0x0F

static uint32_t AP3216C_WriteByteData(uint8_t byte)
{
    WifiIotI2cData i2cData = {0};

    i2cData.sendBuf = &byte;
    i2cData.sendLen = 1;
    return I2cWrite(AP3216C_I2C_IDX, AP3216C_I2C_ADDR, &i2cData);
}

static uint32_t AP3216C_RecvData(uint8_t *data, size_t size)
{
    WifiIotI2cData i2cData = {0};

    i2cData.receiveBuf = data;
    i2cData.receiveLen = size;
    return I2cRead(AP3216C_I2C_IDX, AP3216C_I2C_ADDR, &i2cData);
}

static uint32_t AP3216C_WriteReg(uint8_t regAddr, uint8_t value)
{
    uint8_t buffer[] = {regAddr, value};
    WifiIotI2cData i2cData = {0};

    i2cData.sendBuf = buffer;
    i2cData.sendLen = sizeof(buffer);
    return I2cWrite(AP3216C_I2C_IDX, AP3216C_I2C_ADDR, &i2cData);
}

static uint32_t AP3216C_ReadReg(uint8_t regAddr, uint8_t *value)
{
    uint32_t result;

    result = AP3216C_WriteByteData(regAddr);
    if (result != 0) {
        return result;
    }
    return AP3216C_RecvData(value, 1);
}

uint32_t AP3216C_ReadData(uint16_t *irData, uint16_t *alsData,
                          uint16_t *psData)
{
    uint32_t result;
    uint8_t dataHigh;
    uint8_t dataLow;

    result = AP3216C_ReadReg(AP3216C_IR_L_ADDR, &dataLow);
    if (result != 0) {
        return result;
    }
    result = AP3216C_ReadReg(AP3216C_IR_H_ADDR, &dataHigh);
    if (result != 0) {
        return result;
    }
    *irData = (dataLow & 0x80) ? 0 :
        (((uint16_t)dataHigh << 2) | (dataLow & 0x03));

    result = AP3216C_ReadReg(AP3216C_ALS_L_ADDR, &dataLow);
    if (result != 0) {
        return result;
    }
    result = AP3216C_ReadReg(AP3216C_ALS_H_ADDR, &dataHigh);
    if (result != 0) {
        return result;
    }
    *alsData = ((uint16_t)dataHigh << 8) | dataLow;

    result = AP3216C_ReadReg(AP3216C_PS_L_ADDR, &dataLow);
    if (result != 0) {
        return result;
    }
    result = AP3216C_ReadReg(AP3216C_PS_H_ADDR, &dataHigh);
    if (result != 0) {
        return result;
    }
    *psData = (dataLow & 0x40) ? 0 :
        (((uint16_t)(dataHigh & 0x3F) << 4) | (dataLow & 0x0F));
    return 0;
}

uint32_t AP3216C_Init(void)
{
    uint32_t result;

    GpioInit();
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_10,
              WIFI_IOT_IO_FUNC_GPIO_10_I2C0_SDA);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_9,
              WIFI_IOT_IO_FUNC_GPIO_9_I2C0_SCL);

    I2cInit(WIFI_IOT_I2C_IDX_0, 400000);
    I2cSetBaudrate(WIFI_IOT_I2C_IDX_0, 400000);

    result = AP3216C_WriteReg(AP3216C_SYSTEM_ADDR, 0x04);
    if (result != 0) {
        return result;
    }
    usleep(5000);

    return AP3216C_WriteReg(AP3216C_SYSTEM_ADDR, 0x03);
}
