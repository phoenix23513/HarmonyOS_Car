#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "hal_bsp_ap3216c.h"
#include "hal_bsp_sht20.h"
#include "hi_time.h"
#include "ohos_init.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_watchdog.h"

#define TCRT_LEFT_GPIO              WIFI_IOT_IO_NAME_GPIO_13
#define TCRT_RIGHT_GPIO             WIFI_IOT_IO_NAME_GPIO_14
#define HCSR04_TRIG_GPIO            WIFI_IOT_IO_NAME_GPIO_7
#define HCSR04_ECHO_GPIO            WIFI_IOT_IO_NAME_GPIO_8
#define HCSR04_TIMEOUT_US           30000U
#define HCSR04_TRIGGER_US           20U
#define INVALID_DISTANCE_CM         (-1.0f)
/* 初始串口验收阈值，后续应根据实车测距结果校准。 */
#define OBSTACLE_DISTANCE_CM        20.0f
#define SAMPLE_INTERVAL_TICKS       100U
#define TASK_VERSION                "MULTI_SENSOR_V1"

static void SensorGpioInit(void)
{
    GpioInit();

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_13,
              WIFI_IOT_IO_FUNC_GPIO_13_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_14,
              WIFI_IOT_IO_FUNC_GPIO_14_GPIO);
    GpioSetDir(TCRT_LEFT_GPIO, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(TCRT_RIGHT_GPIO, WIFI_IOT_GPIO_DIR_IN);

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_7,
              WIFI_IOT_IO_FUNC_GPIO_7_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_8,
              WIFI_IOT_IO_FUNC_GPIO_8_GPIO);
    GpioSetDir(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetDir(HCSR04_ECHO_GPIO, WIFI_IOT_GPIO_DIR_IN);
    GpioSetOutputVal(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);
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

static float Hcsr04ReadDistance(void)
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

    if (echoTime == 0 || echoTime >= HCSR04_TIMEOUT_US) {
        return INVALID_DISTANCE_CM;
    }
    return echoTime * 0.034f / 2.0f;
}

static int TcrtRead(WifiIotGpioValue *left, WifiIotGpioValue *right)
{
    if (GpioGetInputVal(TCRT_LEFT_GPIO, left) != WIFI_IOT_SUCCESS) {
        return 0;
    }
    if (GpioGetInputVal(TCRT_RIGHT_GPIO, right) != WIFI_IOT_SUCCESS) {
        return 0;
    }
    return 1;
}

static const char *TcrtStateText(WifiIotGpioValue value)
{
    /* 当前课程硬件定义：低电平为黑色，高电平为白色。 */
    return value == WIFI_IOT_GPIO_VALUE0 ? "BLACK" : "WHITE";
}

static void PrintSample(uint32_t sequence, int tcrtOk,
                        WifiIotGpioValue left, WifiIotGpioValue right,
                        float distance, uint32_t shtStatus,
                        float temperature, float humidity,
                        uint32_t apStatus, uint16_t ir,
                        uint16_t als, uint16_t ps)
{
    int sensorError = !tcrtOk || distance < 0.0f ||
                      shtStatus != 0 || apStatus != 0;
    const char *status = sensorError ? "SENSOR_ERROR" :
        (distance < OBSTACLE_DISTANCE_CM ? "OBSTACLE" : "NORMAL");

    printf("[%s][%06u] ", TASK_VERSION, sequence);

    if (tcrtOk) {
        printf("L=%s R=%s ", TcrtStateText(left), TcrtStateText(right));
    } else {
        printf("L=ERR R=ERR ");
    }

    if (distance >= 0.0f) {
        printf("DIST=%.1fcm ", distance);
    } else {
        printf("DIST=ERR ");
    }

    if (shtStatus == 0) {
        printf("TEMP=%.1fC HUM=%.1f%% ", temperature, humidity);
    } else {
        printf("TEMP=ERR HUM=ERR ");
    }

    if (apStatus == 0) {
        printf("ALS=%u PS=%u IR=%u ", (unsigned int)als,
               (unsigned int)ps, (unsigned int)ir);
    } else {
        printf("ALS=ERR PS=ERR IR=ERR ");
    }

    printf("STATUS=%s\r\n", status);
}

static void MultiSensorTask(void *argument)
{
    uint32_t sequence = 0;
    uint32_t shtInitStatus;
    uint32_t apInitStatus;

    (void)argument;
    SensorGpioInit();

    /* 两个器件共用 I2C0，按顺序访问，不创建并发采集线程。 */
    shtInitStatus = SHT20_Init();
    apInitStatus = AP3216C_Init();
    printf("[%s][BOOT] SHT20_INIT=0x%x AP3216C_INIT=0x%x\r\n",
           TASK_VERSION, shtInitStatus, apInitStatus);

    while (1) {
        WifiIotGpioValue left = WIFI_IOT_GPIO_VALUE0;
        WifiIotGpioValue right = WIFI_IOT_GPIO_VALUE0;
        float distance = Hcsr04ReadDistance();
        float temperature = 0.0f;
        float humidity = 0.0f;
        uint16_t ir = 0;
        uint16_t als = 0;
        uint16_t ps = 0;
        int tcrtOk = TcrtRead(&left, &right);
        uint32_t shtStatus = shtInitStatus;
        uint32_t apStatus = apInitStatus;

        if (shtStatus == 0) {
            shtStatus = SHT20_ReadData(&temperature, &humidity);
        }
        if (apStatus == 0) {
            apStatus = AP3216C_ReadData(&ir, &als, &ps);
        }

        sequence++;
        PrintSample(sequence, tcrtOk, left, right, distance,
                    shtStatus, temperature, humidity,
                    apStatus, ir, als, ps);
        osDelay(SAMPLE_INTERVAL_TICKS);
    }
}

static void MultiSensorMonitor(void)
{
    osThreadAttr_t attr = {0};

    WatchDogDisable();
    attr.name = "multi_sensor";
    attr.stack_size = 10240;
    attr.priority = osPriorityNormal;

    if (osThreadNew(MultiSensorTask, NULL, &attr) == NULL) {
        printf("[%s][BOOT] TASK_CREATE=FAILED\r\n", TASK_VERSION);
    }
}

APP_FEATURE_INIT(MultiSensorMonitor);
