#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_watchdog.h"

#define IR_LEFT_GPIO WIFI_IOT_IO_NAME_GPIO_13
#define IR_RIGHT_GPIO WIFI_IOT_IO_NAME_GPIO_14
#define SAMPLE_TICKS 1U
#define SAMPLES_PER_REPORT 20U
#define TASK_VERSION "WHITE_BOUNDARY_SERIAL_TEST_V1"

static int ReadIrSensors(WifiIotGpioValue *left,
                         WifiIotGpioValue *right)
{
    if (GpioGetInputVal(IR_LEFT_GPIO, left) != WIFI_IOT_SUCCESS) {
        return 0;
    }
    if (GpioGetInputVal(IR_RIGHT_GPIO, right) != WIFI_IOT_SUCCESS) {
        return 0;
    }
    return 1;
}

static void WhiteBoundarySerialTask(void *argument)
{
    WifiIotGpioValue left = WIFI_IOT_GPIO_VALUE0;
    WifiIotGpioValue right = WIFI_IOT_GPIO_VALUE0;
    WifiIotGpioValue previousLeft = WIFI_IOT_GPIO_VALUE0;
    WifiIotGpioValue previousRight = WIFI_IOT_GPIO_VALUE0;
    uint32_t leftOnes = 0;
    uint32_t rightOnes = 0;
    uint32_t samples = 0;
    int previousValid = 0;

    (void)argument;
    printf("[%s][BOOT] MOTOR_DISABLED GPIO13=LEFT GPIO14=RIGHT\r\n",
           TASK_VERSION);
    printf("[%s][TEST] HOLD_SENSORS_OVER FLOOR WHITE BLACK\r\n",
           TASK_VERSION);

    while (1) {
        if (!ReadIrSensors(&left, &right)) {
            printf("[%s][ERROR] GPIO_READ_FAILED\r\n", TASK_VERSION);
            osDelay(SAMPLE_TICKS);
            continue;
        }

        if (previousValid &&
            (left != previousLeft || right != previousRight)) {
            printf("[%s][CHANGE] L:%u->%u R:%u->%u\r\n",
                   TASK_VERSION, (unsigned int)previousLeft,
                   (unsigned int)left, (unsigned int)previousRight,
                   (unsigned int)right);
        }
        previousLeft = left;
        previousRight = right;
        previousValid = 1;

        if (left == WIFI_IOT_GPIO_VALUE1) {
            leftOnes++;
        }
        if (right == WIFI_IOT_GPIO_VALUE1) {
            rightOnes++;
        }
        samples++;

        if (samples >= SAMPLES_PER_REPORT) {
            printf("[%s][RAW] L1=%u/%u R1=%u/%u LAST=%u/%u\r\n",
                   TASK_VERSION, (unsigned int)leftOnes,
                   SAMPLES_PER_REPORT, (unsigned int)rightOnes,
                   SAMPLES_PER_REPORT, (unsigned int)left,
                   (unsigned int)right);
            leftOnes = 0;
            rightOnes = 0;
            samples = 0;
        }

        osDelay(SAMPLE_TICKS);
    }
}

static void WhiteBoundarySerialTest(void)
{
    osThreadAttr_t attr = {0};

    WatchDogDisable();
    GpioInit();

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_13,
              WIFI_IOT_IO_FUNC_GPIO_13_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_14,
              WIFI_IOT_IO_FUNC_GPIO_14_GPIO);
    GpioSetDir(IR_LEFT_GPIO, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(IR_RIGHT_GPIO, WIFI_IOT_GPIO_DIR_IN);

    attr.name = "white_ir_serial_test";
    attr.stack_size = 3072U;
    attr.priority = osPriorityNormal;
    if (osThreadNew(WhiteBoundarySerialTask, NULL, &attr) == NULL) {
        printf("[%s][BOOT] TASK_CREATE_FAILED\r\n", TASK_VERSION);
    }
}

APP_FEATURE_INIT(WhiteBoundarySerialTest);
