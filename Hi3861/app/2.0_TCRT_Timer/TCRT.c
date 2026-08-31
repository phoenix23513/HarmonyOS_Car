#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hi_io.h"
#include "hi_time.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"

#define GPIOL 13
#define GPIOR 14

#define TCRT_SCAN_PERIOD_TICKS 200U
#define HELLO_PERIOD_TICKS 300U

static osTimerId_t g_tcrtTimerId;
static osTimerId_t g_helloTimerId;

static void GetTcrt5000Value(void)
{
    WifiIotGpioValue leftStatus;
    WifiIotGpioValue rightStatus;

    GpioGetInputVal(GPIOL, &leftStatus);
    if (leftStatus == WIFI_IOT_GPIO_VALUE0) {
        printf("left black\r\n");
    } else {
        printf("left white\r\n");
    }

    GpioGetInputVal(GPIOR, &rightStatus);
    if (rightStatus == WIFI_IOT_GPIO_VALUE0) {
        printf("right black\r\n");
    } else {
        printf("right white\r\n");
    }
}

static void TcrtTimerCallback(void *argument)
{
    (void)argument;
    GetTcrt5000Value();
}

static void HelloTimerCallback(void *argument)
{
    (void)argument;
    printf("hello QST\r\n");
}

static void TCRTTask(void *argument)
{
    osStatus_t status;

    (void)argument;
    printf("start test tcrt5000\r\n");

    g_tcrtTimerId = osTimerNew(
        TcrtTimerCallback,
        osTimerPeriodic,
        NULL,
        NULL
    );
    if (g_tcrtTimerId == NULL) {
        printf("TCRT timer could not be created\r\n");
        return;
    }

    g_helloTimerId = osTimerNew(
        HelloTimerCallback,
        osTimerPeriodic,
        NULL,
        NULL
    );
    if (g_helloTimerId == NULL) {
        printf("Hello timer could not be created\r\n");
        return;
    }

    status = osTimerStart(g_tcrtTimerId, TCRT_SCAN_PERIOD_TICKS);
    if (status != osOK) {
        printf("TCRT timer could not be started\r\n");
        return;
    }

    status = osTimerStart(g_helloTimerId, HELLO_PERIOD_TICKS);
    if (status != osOK) {
        printf("Hello timer could not be started\r\n");
        return;
    }

    printf("Timers started: TCRT=2s, hello QST=3s\r\n");
}

static void TCRT(void)
{
    osThreadAttr_t attr;

    GpioInit();

    IoSetFunc(
        WIFI_IOT_IO_NAME_GPIO_13,
        WIFI_IOT_IO_FUNC_GPIO_13_GPIO
    );
    IoSetFunc(
        WIFI_IOT_IO_NAME_GPIO_14,
        WIFI_IOT_IO_FUNC_GPIO_14_GPIO
    );

    GpioSetDir(
        WIFI_IOT_IO_NAME_GPIO_13,
        WIFI_IOT_GPIO_DIR_IN
    );
    GpioSetDir(
        WIFI_IOT_IO_NAME_GPIO_14,
        WIFI_IOT_GPIO_DIR_IN
    );

    attr.name = "TCRTTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 10240U;
    attr.priority = 25;

    if (osThreadNew(TCRTTask, NULL, &attr) == NULL) {
        printf("Failed to create TCRTTask!\r\n");
    }
}

APP_FEATURE_INIT(TCRT);
