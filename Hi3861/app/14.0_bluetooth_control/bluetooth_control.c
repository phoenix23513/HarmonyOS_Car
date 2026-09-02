#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include "wifiiot_uart.h"
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifiiot_gpio.h"
#include "hi_io.h"
#include "hi_time.h"
#include "wifiiot_pwm.h"
#include "hi_pwm.h"
#include "hi_uart.h"
#include "wifiiot_gpio_ex.h"
#include "hi_task.h"
#include "wifiiot_errno.h"

uint8_t uart_sendbuf[20];
uint8_t bluetooth_flag[1000];

void stm32motor_control(int motorA, int motorB)
{
    uint8_t A_dir = 0;
    uint8_t B_dir = 0;

    if (motorA < 0) {
        A_dir = 1;
        motorA = -motorA;
    }
    if (motorB < 0) {
        B_dir = 1;
        motorB = -motorB;
    }
    if (motorA > 150) {
        motorA = 150;
    }
    if (motorB > 150) {
        motorB = 150;
    }

    uart_sendbuf[0] = 0xFC;
    uart_sendbuf[1] = A_dir;
    uart_sendbuf[2] = motorA;
    uart_sendbuf[3] = B_dir;
    uart_sendbuf[4] = motorB;
    uart_sendbuf[5] = 0xFD;
    UartWrite(WIFI_IOT_UART_IDX_2, (unsigned char *)uart_sendbuf, 6);
}

void car_backward(void)
{
    stm32motor_control(-150, -150);
}

void car_forward(void)
{
    stm32motor_control(100, 100);
}

void car_left(void)
{
    stm32motor_control(-50, 150);
}

void car_right(void)
{
    stm32motor_control(150, -50);
}

void car_stop(void)
{
    stm32motor_control(0, 0);
}

static void car_mode_bluetooth(void)
{
    while (1) {
        UartRead(WIFI_IOT_UART_IDX_1, bluetooth_flag, 1000);
        if (bluetooth_flag[0] != 0) {
            switch (bluetooth_flag[0]) {
                case 'O':
                    car_stop();
                    break;
                case 'W':
                    car_forward();
                    break;
                case 'A':
                    car_left();
                    break;
                case 'D':
                    car_right();
                    break;
                case 'S':
                    car_backward();
                    break;
                case 'I':
                    stm32motor_control(100, 100);
                    break;
                case 'K':
                    stm32motor_control(150, 150);
                    break;
                default:
                    break;
            }
            bluetooth_flag[0] = 0;
        }
        hi_sleep(50);
    }
}

static void Control(void)
{
    uint32_t ret;

    GpioInit();

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_0,
              WIFI_IOT_IO_FUNC_GPIO_0_UART1_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_1,
              WIFI_IOT_IO_FUNC_GPIO_1_UART1_RXD);
    WifiIotUartAttribute uart_attr1 = {
        .baudRate = 9600,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };

    ret = UartInit(WIFI_IOT_UART_IDX_1, &uart_attr1, NULL);
    if (ret != WIFI_IOT_SUCCESS) {
        printf("Failed to init bluetooth uart! Err code = %u\n", ret);
        return;
    }
    printf("ble uart OK!\r\n");

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11,
              WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12,
              WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);
    WifiIotUartAttribute uart_attr2 = {
        .baudRate = 115200,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };

    ret = UartInit(WIFI_IOT_UART_IDX_2, &uart_attr2, NULL);
    if (ret != WIFI_IOT_SUCCESS) {
        printf("Failed to init stm32 uart! Err code = %u\n", ret);
        return;
    }
    printf("uart OK!\r\n");

    osThreadAttr_t attr;
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 4;
    attr.name = "car_mode_bluetooth";
    attr.priority = 25;

    if (osThreadNew((osThreadFunc_t)car_mode_bluetooth, NULL, &attr) == NULL) {
        printf("Falied to create car_mode_bluetooth!\n");
    }
}

APP_FEATURE_INIT(Control);
