#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "hi_time.h"
#include "ohos_init.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_watchdog.h"

#define SERVO_GPIO                  WIFI_IOT_IO_NAME_GPIO_2
#define HCSR04_TRIG_GPIO            WIFI_IOT_IO_NAME_GPIO_7
#define HCSR04_ECHO_GPIO            WIFI_IOT_IO_NAME_GPIO_8

#define SERVO_PERIOD_US             20000U
#define SERVO_PULSE_COUNT           25U
#define SERVO_SETTLE_TICKS          50U
#define POSITION_HOLD_TICKS         150U

#define HCSR04_TRIGGER_US           20U
#define HCSR04_TIMEOUT_US           30000U
#define DISTANCE_SAMPLE_COUNT       3U
#define DISTANCE_SAMPLE_TICKS       10U
#define INVALID_DISTANCE_CM         (-1.0f)

#define TASK_VERSION                "SG90_CAL_V1"

typedef struct {
    const char *name;
    uint32_t pulseUs;
} ServoPosition;

static const ServoPosition g_positions[] = {
    {"CENTER", 1500U},
    {"LEFT_MID", 2000U},
    {"LEFT_LIMIT_CANDIDATE", 2400U},
    {"CENTER", 1500U},
    {"RIGHT_MID", 1000U},
    {"RIGHT_LIMIT_CANDIDATE", 600U},
    {"CENTER", 1500U},
};

static void ServoWritePulse(uint32_t pulseUs)
{
    GpioSetOutputVal(SERVO_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(pulseUs);
    GpioSetOutputVal(SERVO_GPIO, WIFI_IOT_GPIO_VALUE0);
    hi_udelay(SERVO_PERIOD_US - pulseUs);
}

static void ServoMoveTo(uint32_t pulseUs)
{
    uint32_t i;

    for (i = 0; i < SERVO_PULSE_COUNT; i++) {
        ServoWritePulse(pulseUs);
    }
}

static int WaitForEchoLevel(WifiIotGpioValue expectedLevel,
                            uint32_t timeoutUs)
{
    uint32_t startTime = hi_get_us();
    WifiIotGpioValue value = WIFI_IOT_GPIO_VALUE0;

    while ((uint32_t)(hi_get_us() - startTime) < timeoutUs) {
        if (GpioGetInputVal(HCSR04_ECHO_GPIO, &value) !=
            WIFI_IOT_SUCCESS) {
            return 0;
        }
        if (value == expectedLevel) {
            return 1;
        }
    }
    return 0;
}

static float ReadDistanceCm(void)
{
    uint32_t echoStart;
    uint32_t echoTime;

    if (!WaitForEchoLevel(WIFI_IOT_GPIO_VALUE0, HCSR04_TIMEOUT_US)) {
        return INVALID_DISTANCE_CM;
    }

    GpioSetOutputVal(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(HCSR04_TRIGGER_US);
    GpioSetOutputVal(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);

    if (!WaitForEchoLevel(WIFI_IOT_GPIO_VALUE1, HCSR04_TIMEOUT_US)) {
        return INVALID_DISTANCE_CM;
    }
    echoStart = hi_get_us();

    if (!WaitForEchoLevel(WIFI_IOT_GPIO_VALUE0, HCSR04_TIMEOUT_US)) {
        return INVALID_DISTANCE_CM;
    }
    echoTime = (uint32_t)(hi_get_us() - echoStart);
    if (echoTime == 0U || echoTime >= HCSR04_TIMEOUT_US) {
        return INVALID_DISTANCE_CM;
    }

    return echoTime * 0.034f / 2.0f;
}

static void PrintDirectionDistances(const ServoPosition *position)
{
    uint32_t i;

    for (i = 0; i < DISTANCE_SAMPLE_COUNT; i++) {
        float distance = ReadDistanceCm();

        if (distance < 0.0f) {
            printf("[%s][DIST] POS=%s PWM=%u SAMPLE=%u DIST=ERR\r\n",
                   TASK_VERSION, position->name,
                   (unsigned int)position->pulseUs,
                   (unsigned int)(i + 1U));
        } else {
            printf("[%s][DIST] POS=%s PWM=%u SAMPLE=%u DIST=%.1fcm\r\n",
                   TASK_VERSION, position->name,
                   (unsigned int)position->pulseUs,
                   (unsigned int)(i + 1U), distance);
        }
        osDelay(DISTANCE_SAMPLE_TICKS);
    }
}

static void CalibrationTask(void *argument)
{
    uint32_t i;

    (void)argument;
    printf("[%s][BOOT] THREE_DIRECTION_SERVO_CALIBRATION\r\n",
           TASK_VERSION);
    printf("[%s][INFO] Stop immediately if the servo buzzes or jams.\r\n",
           TASK_VERSION);

    while (1) {
        for (i = 0; i < sizeof(g_positions) / sizeof(g_positions[0]); i++) {
            printf("[%s][MOVE] POS=%s PWM=%u\r\n", TASK_VERSION,
                   g_positions[i].name,
                   (unsigned int)g_positions[i].pulseUs);
            ServoMoveTo(g_positions[i].pulseUs);
            osDelay(SERVO_SETTLE_TICKS);
            PrintDirectionDistances(&g_positions[i]);
            osDelay(POSITION_HOLD_TICKS);
        }
    }
}

static void Sg90Calibration(void)
{
    osThreadAttr_t attr = {0};

    WatchDogDisable();
    GpioInit();

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_2,
              WIFI_IOT_IO_FUNC_GPIO_2_GPIO);
    GpioSetDir(SERVO_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetOutputVal(SERVO_GPIO, WIFI_IOT_GPIO_VALUE0);

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_7,
              WIFI_IOT_IO_FUNC_GPIO_7_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_8,
              WIFI_IOT_IO_FUNC_GPIO_8_GPIO);
    GpioSetDir(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetDir(HCSR04_ECHO_GPIO, WIFI_IOT_GPIO_DIR_IN);
    GpioSetOutputVal(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);

    attr.name = "sg90_calibration";
    attr.stack_size = 4096U;
    attr.priority = osPriorityNormal;
    if (osThreadNew(CalibrationTask, NULL, &attr) == NULL) {
        printf("[%s][BOOT] TASK_CREATE_FAILED\r\n", TASK_VERSION);
    }
}

APP_FEATURE_INIT(Sg90Calibration);
