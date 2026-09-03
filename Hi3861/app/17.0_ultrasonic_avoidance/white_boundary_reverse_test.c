#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"
#include "wifiiot_watchdog.h"

#define IR_LEFT_GPIO WIFI_IOT_IO_NAME_GPIO_13
#define IR_RIGHT_GPIO WIFI_IOT_IO_NAME_GPIO_14

#define FRAME_HEAD 0xFC
#define FRAME_TAIL 0xFD
#define FORWARD_LEFT_SPEED 50
#define FORWARD_RIGHT_SPEED 48
#define REVERSE_SPEED 35

#define SENSOR_SAMPLE_TICKS 2U
#define MOTOR_REFRESH_TICKS 10U
#define STATUS_PRINT_TICKS 20U
#define START_DELAY_TICKS 300U
#define BASELINE_SAMPLE_COUNT 30U
#define BASELINE_STABLE_PERCENT 80U
#define BOUNDARY_CONFIRM_SAMPLES 5U
#define BOUNDARY_CONFIRM_REQUIRED 3U
#define SAFE_CONFIRM_COUNT 5U
#define MAX_REVERSE_SEARCH_TICKS 200U
#define EXTRA_REVERSE_TICKS 80U
#define TASK_VERSION "WHITE_BOUNDARY_REVERSE_TEST_V1"

static uint8_t g_motorFrame[6];
static WifiIotGpioValue g_leftFloorLevel = WIFI_IOT_GPIO_VALUE0;
static WifiIotGpioValue g_rightFloorLevel = WIFI_IOT_GPIO_VALUE0;

static void SendMotorCommand(int left, int right)
{
    uint8_t leftDirection = 0;
    uint8_t rightDirection = 0;

    if (left < 0) {
        leftDirection = 1;
        left = -left;
    }
    if (right < 0) {
        rightDirection = 1;
        right = -right;
    }

    g_motorFrame[0] = FRAME_HEAD;
    g_motorFrame[1] = leftDirection;
    g_motorFrame[2] = (uint8_t)left;
    g_motorFrame[3] = rightDirection;
    g_motorFrame[4] = (uint8_t)right;
    g_motorFrame[5] = FRAME_TAIL;
    UartWrite(WIFI_IOT_UART_IDX_2, g_motorFrame, sizeof(g_motorFrame));
}

static void StopMotor(void)
{
    SendMotorCommand(0, 0);
}

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

static int IsFloor(WifiIotGpioValue left, WifiIotGpioValue right)
{
    return left == g_leftFloorLevel && right == g_rightFloorLevel;
}

static int CalibrateFloor(void)
{
    WifiIotGpioValue left;
    WifiIotGpioValue right;
    uint32_t leftOnes = 0;
    uint32_t rightOnes = 0;
    uint32_t i;
    uint32_t leftStable;
    uint32_t rightStable;

    printf("[%s][CALIBRATE] KEEP_BOTH_SENSORS_ON_NORMAL_FLOOR\r\n",
           TASK_VERSION);
    for (i = 0; i < BASELINE_SAMPLE_COUNT; i++) {
        if (!ReadIrSensors(&left, &right)) {
            printf("[%s][ERROR] GPIO_READ_FAILED\r\n", TASK_VERSION);
            return 0;
        }
        if (left == WIFI_IOT_GPIO_VALUE1) {
            leftOnes++;
        }
        if (right == WIFI_IOT_GPIO_VALUE1) {
            rightOnes++;
        }
        osDelay(SENSOR_SAMPLE_TICKS);
    }

    g_leftFloorLevel = leftOnes * 2U >= BASELINE_SAMPLE_COUNT ?
                       WIFI_IOT_GPIO_VALUE1 : WIFI_IOT_GPIO_VALUE0;
    g_rightFloorLevel = rightOnes * 2U >= BASELINE_SAMPLE_COUNT ?
                        WIFI_IOT_GPIO_VALUE1 : WIFI_IOT_GPIO_VALUE0;
    leftStable = g_leftFloorLevel == WIFI_IOT_GPIO_VALUE1 ?
                 leftOnes : BASELINE_SAMPLE_COUNT - leftOnes;
    rightStable = g_rightFloorLevel == WIFI_IOT_GPIO_VALUE1 ?
                  rightOnes : BASELINE_SAMPLE_COUNT - rightOnes;

    printf("[%s][BASELINE] LEFT=%u RIGHT=%u STABLE=%u/%u,%u/%u\r\n",
           TASK_VERSION, (unsigned int)g_leftFloorLevel,
           (unsigned int)g_rightFloorLevel, (unsigned int)leftStable,
           BASELINE_SAMPLE_COUNT, (unsigned int)rightStable,
           BASELINE_SAMPLE_COUNT);

    if (leftStable * 100U <
            BASELINE_SAMPLE_COUNT * BASELINE_STABLE_PERCENT ||
        rightStable * 100U <
            BASELINE_SAMPLE_COUNT * BASELINE_STABLE_PERCENT) {
        printf("[%s][ERROR] FLOOR_BASELINE_UNSTABLE\r\n", TASK_VERSION);
        return 0;
    }
    return 1;
}

static int ConfirmBoundary(void)
{
    WifiIotGpioValue left;
    WifiIotGpioValue right;
    uint32_t detected = 0;
    uint32_t i;

    for (i = 0; i < BOUNDARY_CONFIRM_SAMPLES; i++) {
        if (!ReadIrSensors(&left, &right)) {
            return -1;
        }
        if (!IsFloor(left, right)) {
            detected++;
        }
        osDelay(SENSOR_SAMPLE_TICKS);
    }
    return detected >= BOUNDARY_CONFIRM_REQUIRED;
}

static int ReverseOutOfBoundary(void)
{
    WifiIotGpioValue left;
    WifiIotGpioValue right;
    uint32_t elapsed = 0;
    uint32_t motorRefresh = MOTOR_REFRESH_TICKS;
    uint32_t safeCount = 0;

    printf("[%s][ACTION] REVERSE_ONLY SPEED=%d\r\n",
           TASK_VERSION, REVERSE_SPEED);
    while (elapsed < MAX_REVERSE_SEARCH_TICKS) {
        if (motorRefresh >= MOTOR_REFRESH_TICKS) {
            SendMotorCommand(-REVERSE_SPEED, -REVERSE_SPEED);
            motorRefresh = 0;
        }
        if (!ReadIrSensors(&left, &right)) {
            StopMotor();
            printf("[%s][ERROR] GPIO_READ_FAILED STOPPED\r\n",
                   TASK_VERSION);
            return 0;
        }

        if (IsFloor(left, right)) {
            safeCount++;
        } else {
            safeCount = 0;
        }
        if (safeCount >= SAFE_CONFIRM_COUNT) {
            uint32_t extra = 0;

            printf("[%s][SAFE] WHITE_CLEARED EXTRA_REVERSE=%u\r\n",
                   TASK_VERSION, EXTRA_REVERSE_TICKS);
            while (extra < EXTRA_REVERSE_TICKS) {
                SendMotorCommand(-REVERSE_SPEED, -REVERSE_SPEED);
                osDelay(MOTOR_REFRESH_TICKS);
                extra += MOTOR_REFRESH_TICKS;
            }
            StopMotor();
            printf("[%s][DONE] REVERSE_COMPLETE TEST_STOPPED\r\n",
                   TASK_VERSION);
            return 1;
        }

        osDelay(SENSOR_SAMPLE_TICKS);
        elapsed += SENSOR_SAMPLE_TICKS;
        motorRefresh += SENSOR_SAMPLE_TICKS;
    }

    StopMotor();
    printf("[%s][ERROR] REVERSE_TIMEOUT TEST_STOPPED\r\n",
           TASK_VERSION);
    return 0;
}

static void WhiteBoundaryTestTask(void *argument)
{
    WifiIotGpioValue left;
    WifiIotGpioValue right;
    uint32_t motorRefresh = MOTOR_REFRESH_TICKS;
    uint32_t statusRefresh = STATUS_PRINT_TICKS;

    (void)argument;
    StopMotor();
    printf("[%s][BOOT] PLACE_CAR_ON_NORMAL_FLOOR\r\n", TASK_VERSION);
    printf("[%s][WAIT] TEST_STARTS_IN_3_SECONDS\r\n", TASK_VERSION);
    osDelay(START_DELAY_TICKS);

    if (!CalibrateFloor()) {
        StopMotor();
        while (1) {
            osDelay(STATUS_PRINT_TICKS);
        }
    }

    printf("[%s][ACTION] FORWARD SPEED=%d/%d\r\n", TASK_VERSION,
           FORWARD_LEFT_SPEED, FORWARD_RIGHT_SPEED);
    while (1) {
        int confirmed;

        if (!ReadIrSensors(&left, &right)) {
            StopMotor();
            printf("[%s][ERROR] GPIO_READ_FAILED STOPPED\r\n",
                   TASK_VERSION);
            osDelay(SENSOR_SAMPLE_TICKS);
            continue;
        }

        if (!IsFloor(left, right)) {
            StopMotor();
            printf("[%s][SUSPECT] STOP L=%u R=%u\r\n", TASK_VERSION,
                   (unsigned int)left, (unsigned int)right);
            confirmed = ConfirmBoundary();
            if (confirmed < 0) {
                StopMotor();
                printf("[%s][ERROR] GPIO_READ_FAILED STOPPED\r\n",
                       TASK_VERSION);
                break;
            }
            if (confirmed > 0) {
                printf("[%s][BOUNDARY] WHITE_CONFIRMED\r\n",
                       TASK_VERSION);
                ReverseOutOfBoundary();
                break;
            }
            printf("[%s][FALSE_TRIGGER] RESUME_FORWARD\r\n",
                   TASK_VERSION);
            motorRefresh = MOTOR_REFRESH_TICKS;
        }

        if (motorRefresh >= MOTOR_REFRESH_TICKS) {
            SendMotorCommand(FORWARD_LEFT_SPEED, FORWARD_RIGHT_SPEED);
            motorRefresh = 0;
        }
        if (statusRefresh >= STATUS_PRINT_TICKS) {
            printf("[%s][SENSOR] L=%u R=%u FLOOR=%u/%u\r\n",
                   TASK_VERSION, (unsigned int)left,
                   (unsigned int)right,
                   (unsigned int)g_leftFloorLevel,
                   (unsigned int)g_rightFloorLevel);
            statusRefresh = 0;
        }

        osDelay(SENSOR_SAMPLE_TICKS);
        motorRefresh += SENSOR_SAMPLE_TICKS;
        statusRefresh += SENSOR_SAMPLE_TICKS;
    }

    StopMotor();
    while (1) {
        osDelay(STATUS_PRINT_TICKS);
    }
}

static void WhiteBoundaryReverseTest(void)
{
    WifiIotUartAttribute uart = {
        .baudRate = 115200,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };
    osThreadAttr_t attr = {0};

    WatchDogDisable();
    GpioInit();

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_13,
              WIFI_IOT_IO_FUNC_GPIO_13_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_14,
              WIFI_IOT_IO_FUNC_GPIO_14_GPIO);
    GpioSetDir(IR_LEFT_GPIO, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(IR_RIGHT_GPIO, WIFI_IOT_GPIO_DIR_IN);

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11,
              WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12,
              WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);
    UartInit(WIFI_IOT_UART_IDX_2, &uart, NULL);

    attr.name = "white_boundary_test";
    attr.stack_size = 4096U;
    attr.priority = osPriorityNormal;
    if (osThreadNew(WhiteBoundaryTestTask, NULL, &attr) == NULL) {
        printf("[%s][BOOT] TASK_CREATE_FAILED\r\n", TASK_VERSION);
    }
}

APP_FEATURE_INIT(WhiteBoundaryReverseTest);
