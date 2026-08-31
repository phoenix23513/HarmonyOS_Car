#include "front_light_uart.h"

#include <stddef.h>

#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"

#define FRONT_LIGHT_UART_INDEX WIFI_IOT_UART_IDX_2
#define FRONT_LIGHT_UART_BAUDRATE 115200

#define FRONT_LIGHT_FRAME_HEADER 0xFA
#define FRONT_LIGHT_FRAME_COMMAND 0x4C
#define FRONT_LIGHT_FRAME_FOOTER 0xFB
#define FRONT_LIGHT_FRAME_LENGTH 5

uint32_t FrontLightUartInit(void)
{
    WifiIotUartAttribute attributes = {0};

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12, WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);

    attributes.baudRate = FRONT_LIGHT_UART_BAUDRATE;
    attributes.dataBits = 8;
    attributes.stopBits = 1;
    attributes.parity = 0;

    return UartInit(FRONT_LIGHT_UART_INDEX, &attributes, NULL);
}

uint32_t FrontLightUartSend(uint8_t lightOn)
{
    uint8_t state = lightOn != 0 ? 1 : 0;
    uint8_t frame[FRONT_LIGHT_FRAME_LENGTH] = {
        FRONT_LIGHT_FRAME_HEADER,
        FRONT_LIGHT_FRAME_COMMAND,
        state,
        FRONT_LIGHT_FRAME_COMMAND ^ state,
        FRONT_LIGHT_FRAME_FOOTER,
    };
    int32_t sentBytes;

    sentBytes = UartWrite(FRONT_LIGHT_UART_INDEX, frame, sizeof(frame));
    return sentBytes == FRONT_LIGHT_FRAME_LENGTH ? 0 : 1;
}
