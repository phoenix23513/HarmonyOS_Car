#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"
#include "wifiiot_watchdog.h"

#define EDGE_LEFT_GPIO              WIFI_IOT_IO_NAME_GPIO_13
#define EDGE_RIGHT_GPIO             WIFI_IOT_IO_NAME_GPIO_14
#define MOTOR_FRAME_HEAD            0xFC
#define MOTOR_FRAME_TAIL            0xFD
#define FORWARD_LEFT_SPEED_X100     50
#define FORWARD_RIGHT_SPEED_X100    48
#define REVERSE_SPEED_X100          40
#define IN_PLACE_TURN_SPEED_X100    40
#define SENSOR_SAMPLE_TICKS         2U
#define MOTOR_REFRESH_TICKS         10U
#define STATUS_PRINT_TICKS          20U
#define START_DELAY_TICKS           300U
#define STOP_WAIT_TICKS             10U
#define REVERSE_TIME_TICKS          30U
#define EXTRA_REVERSE_TICKS         60U
#define MAX_REVERSE_SEARCH_TICKS    150U
#define TURN_WAIT_TICKS             10U
#define IN_PLACE_TURN_TICKS         70U
#define EDGE_CONFIRM_COUNT          3U
#define SAFE_CONFIRM_COUNT          5U
#define TASK_VERSION                "EDGE_AUTO_V3"

static uint8_t g_motorFrame[6];
static int g_bothEdgeTurnRight = 1;

static void SendMotorCommand(int leftSpeedX100, int rightSpeedX100)
{
    uint8_t leftDirection = 0;
    uint8_t rightDirection = 0;

    if (leftSpeedX100 < 0) {
        leftDirection = 1;
        leftSpeedX100 = -leftSpeedX100;
    }
    if (rightSpeedX100 < 0) {
        rightDirection = 1;
        rightSpeedX100 = -rightSpeedX100;
    }

    g_motorFrame[0] = MOTOR_FRAME_HEAD;
    g_motorFrame[1] = leftDirection;
    g_motorFrame[2] = (uint8_t)leftSpeedX100;
    g_motorFrame[3] = rightDirection;
    g_motorFrame[4] = (uint8_t)rightSpeedX100;
    g_motorFrame[5] = MOTOR_FRAME_TAIL;
    UartWrite(WIFI_IOT_UART_IDX_2, g_motorFrame, sizeof(g_motorFrame));
}

static int ReadEdgeSensors(WifiIotGpioValue *left,
                           WifiIotGpioValue *right)
{
    if (GpioGetInputVal(EDGE_LEFT_GPIO, left) != WIFI_IOT_SUCCESS) {
        return 0;
    }
    if (GpioGetInputVal(EDGE_RIGHT_GPIO, right) != WIFI_IOT_SUCCESS) {
        return 0;
    }
    return 1;
}

static const char *EdgeText(WifiIotGpioValue left,
                            WifiIotGpioValue right)
{
    if (left == WIFI_IOT_GPIO_VALUE0 &&
        right == WIFI_IOT_GPIO_VALUE0) {
        return "SAFE";
    }
    if (left == WIFI_IOT_GPIO_VALUE1 &&
        right == WIFI_IOT_GPIO_VALUE0) {
        return "LEFT_EDGE";
    }
    if (left == WIFI_IOT_GPIO_VALUE0 &&
        right == WIFI_IOT_GPIO_VALUE1) {
        return "RIGHT_EDGE";
    }
    return "BOTH_EDGE";
}

static void RunMotorFor(int leftSpeedX100, int rightSpeedX100,
                        uint32_t durationTicks)
{
    uint32_t elapsedTicks = 0;

    while (elapsedTicks < durationTicks) {
        SendMotorCommand(leftSpeedX100, rightSpeedX100);
        osDelay(MOTOR_REFRESH_TICKS);
        elapsedTicks += MOTOR_REFRESH_TICKS;
    }
}

static int SelectTurnRight(WifiIotGpioValue left,
                           WifiIotGpioValue right)
{
    int turnRight;

    if (left == WIFI_IOT_GPIO_VALUE1 &&
        right == WIFI_IOT_GPIO_VALUE0) {
        return 1;
    }
    if (left == WIFI_IOT_GPIO_VALUE0 &&
        right == WIFI_IOT_GPIO_VALUE1) {
        return 0;
    }
    turnRight = g_bothEdgeTurnRight;
    g_bothEdgeTurnRight = !g_bothEdgeTurnRight;
    return turnRight;
}

static int ReverseUntilSafe(void)
{
    WifiIotGpioValue left = WIFI_IOT_GPIO_VALUE1;
    WifiIotGpioValue right = WIFI_IOT_GPIO_VALUE1;
    uint32_t elapsedTicks = 0;
    uint32_t motorRefresh = MOTOR_REFRESH_TICKS;
    uint32_t safeCount = 0;

    printf("[%s][ACTION] BACKWARD_UNTIL_SAFE\r\n", TASK_VERSION);
    while (elapsedTicks < MAX_REVERSE_SEARCH_TICKS) {
        if (motorRefresh >= MOTOR_REFRESH_TICKS) {
            SendMotorCommand(-REVERSE_SPEED_X100,
                             -REVERSE_SPEED_X100);
            motorRefresh = 0;
        }
        if (!ReadEdgeSensors(&left, &right)) {
            SendMotorCommand(0, 0);
            printf("[%s][ERROR] GPIO_READ_FAILED, STOPPED\r\n",
                   TASK_VERSION);
            return -1;
        }

        if (left == WIFI_IOT_GPIO_VALUE0 &&
            right == WIFI_IOT_GPIO_VALUE0) {
            safeCount++;
        } else {
            safeCount = 0;
        }

        if (safeCount >= SAFE_CONFIRM_COUNT) {
            printf("[%s][SAFE] L=0 R=0, EXTRA_BACKWARD\r\n",
                   TASK_VERSION);
            RunMotorFor(-REVERSE_SPEED_X100,
                        -REVERSE_SPEED_X100,
                        EXTRA_REVERSE_TICKS);
            RunMotorFor(0, 0, TURN_WAIT_TICKS);
            return 1;
        }

        osDelay(SENSOR_SAMPLE_TICKS);
        elapsedTicks += SENSOR_SAMPLE_TICKS;
        motorRefresh += SENSOR_SAMPLE_TICKS;
    }
    printf("[%s][ERROR] REVERSE_SAFE_TIMEOUT, STOPPED\r\n",
           TASK_VERSION);
    SendMotorCommand(0, 0);
    return 0;
}

static int RunInPlaceTurn(int turnRight, WifiIotGpioValue *left,
                          WifiIotGpioValue *right)
{
    uint32_t elapsedTicks = 0;
    uint32_t motorRefresh = MOTOR_REFRESH_TICKS;
    uint32_t edgeCount = 0;
    int leftSpeed = turnRight ? IN_PLACE_TURN_SPEED_X100 :
                                -IN_PLACE_TURN_SPEED_X100;
    int rightSpeed = turnRight ? -IN_PLACE_TURN_SPEED_X100 :
                                  IN_PLACE_TURN_SPEED_X100;

    printf("[%s][ACTION] %s\r\n", TASK_VERSION,
           turnRight ? "IN_PLACE_RIGHT" : "IN_PLACE_LEFT");
    while (elapsedTicks < IN_PLACE_TURN_TICKS) {
        if (motorRefresh >= MOTOR_REFRESH_TICKS) {
            SendMotorCommand(leftSpeed, rightSpeed);
            motorRefresh = 0;
        }
        if (!ReadEdgeSensors(left, right)) {
            SendMotorCommand(0, 0);
            printf("[%s][ERROR] GPIO_READ_FAILED, STOPPED\r\n",
                   TASK_VERSION);
            return -1;
        }

        if (*left == WIFI_IOT_GPIO_VALUE1 ||
            *right == WIFI_IOT_GPIO_VALUE1) {
            edgeCount++;
        } else {
            edgeCount = 0;
        }
        if (edgeCount >= EDGE_CONFIRM_COUNT) {
            SendMotorCommand(0, 0);
            printf("[%s][EDGE_DURING_TURN] %s L=%u R=%u\r\n",
                   TASK_VERSION, EdgeText(*left, *right),
                   (unsigned int)*left, (unsigned int)*right);
            return 0;
        }

        osDelay(SENSOR_SAMPLE_TICKS);
        elapsedTicks += SENSOR_SAMPLE_TICKS;
        motorRefresh += SENSOR_SAMPLE_TICKS;
    }
    return 1;
}

static int RunAvoidance(WifiIotGpioValue left,
                        WifiIotGpioValue right)
{
    int turnRight = SelectTurnRight(left, right);
    int turnResult;

    printf("[%s][EDGE] %s L=%u R=%u\r\n", TASK_VERSION,
           EdgeText(left, right), (unsigned int)left,
           (unsigned int)right);

    while (1) {
        printf("[%s][ACTION] STOP\r\n", TASK_VERSION);
        RunMotorFor(0, 0, STOP_WAIT_TICKS);

        if (ReverseUntilSafe() <= 0) {
            return 0;
        }

        turnResult = RunInPlaceTurn(turnRight, &left, &right);
        if (turnResult > 0) {
            printf("[%s][ACTION] AVOIDANCE_SUCCESS\r\n",
                   TASK_VERSION);
            return 1;
        }
        if (turnResult < 0) {
            return 0;
        }

        printf("[%s][ACTION] RETRY_ESCAPE\r\n", TASK_VERSION);
        turnRight = SelectTurnRight(left, right);
    }
}

static void EdgeStopTask(void *argument)
{
    WifiIotGpioValue left = WIFI_IOT_GPIO_VALUE0;
    WifiIotGpioValue right = WIFI_IOT_GPIO_VALUE0;
    uint32_t edgeCount = 0;
    uint32_t safeCount = 0;
    uint32_t motorRefresh = 0;
    uint32_t statusRefresh = 0;
    int forwardEnabled = 0;

    (void)argument;
    printf("[%s][BOOT] CONTINUOUS_TABLE_EDGE_AVOIDANCE\r\n",
           TASK_VERSION);
    printf("[%s][INFO] TABLE=0 EDGE=1, L=GPIO13 R=GPIO14\r\n",
           TASK_VERSION);
    SendMotorCommand(0, 0);
    printf("[%s][WAIT] Test starts in 3 seconds\r\n", TASK_VERSION);
    osDelay(START_DELAY_TICKS);

    while (1) {
        if (!ReadEdgeSensors(&left, &right)) {
            SendMotorCommand(0, 0);
            printf("[%s][ERROR] GPIO_READ_FAILED, STOPPED\r\n",
                   TASK_VERSION);
            osDelay(SENSOR_SAMPLE_TICKS);
            continue;
        }

        if (!forwardEnabled) {
            if (left == WIFI_IOT_GPIO_VALUE0 &&
                right == WIFI_IOT_GPIO_VALUE0) {
                safeCount++;
            } else {
                safeCount = 0;
            }

            SendMotorCommand(0, 0);
            if (safeCount >= SAFE_CONFIRM_COUNT) {
                forwardEnabled = 1;
                edgeCount = 0;
                motorRefresh = MOTOR_REFRESH_TICKS;
                printf("[%s][ACTION] FORWARD L=%d R=%d\r\n",
                       TASK_VERSION, FORWARD_LEFT_SPEED_X100,
                       FORWARD_RIGHT_SPEED_X100);
            }
        } else {
            if (left == WIFI_IOT_GPIO_VALUE1 ||
                right == WIFI_IOT_GPIO_VALUE1) {
                edgeCount++;
            } else {
                edgeCount = 0;
            }

            if (edgeCount >= EDGE_CONFIRM_COUNT) {
                if (RunAvoidance(left, right)) {
                    forwardEnabled = 1;
                    printf("[%s][ACTION] FORWARD L=%d R=%d\r\n",
                           TASK_VERSION, FORWARD_LEFT_SPEED_X100,
                           FORWARD_RIGHT_SPEED_X100);
                } else {
                    forwardEnabled = 0;
                    safeCount = 0;
                }
                edgeCount = 0;
                motorRefresh = MOTOR_REFRESH_TICKS;
            } else if (motorRefresh >= MOTOR_REFRESH_TICKS) {
                SendMotorCommand(FORWARD_LEFT_SPEED_X100,
                                 FORWARD_RIGHT_SPEED_X100);
                motorRefresh = 0;
            }
        }

        if (statusRefresh >= STATUS_PRINT_TICKS) {
            printf("[%s][SENSOR] L=%u R=%u STATE=%s\r\n",
                   TASK_VERSION, (unsigned int)left,
                   (unsigned int)right, EdgeText(left, right));
            statusRefresh = 0;
        }

        osDelay(SENSOR_SAMPLE_TICKS);
        motorRefresh += SENSOR_SAMPLE_TICKS;
        statusRefresh += SENSOR_SAMPLE_TICKS;
    }
}

static void TableEdgeTest(void)
{
    WifiIotUartAttribute uartAttribute = {
        .baudRate = 115200,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };
    osThreadAttr_t threadAttribute = {0};

    WatchDogDisable();
    GpioInit();

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_13,
              WIFI_IOT_IO_FUNC_GPIO_13_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_14,
              WIFI_IOT_IO_FUNC_GPIO_14_GPIO);
    GpioSetDir(EDGE_LEFT_GPIO, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(EDGE_RIGHT_GPIO, WIFI_IOT_GPIO_DIR_IN);

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11,
              WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12,
              WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);
    UartInit(WIFI_IOT_UART_IDX_2, &uartAttribute, NULL);

    threadAttribute.name = "edge_stop_test";
    threadAttribute.stack_size = 3072U;
    threadAttribute.priority = osPriorityNormal;
    if (osThreadNew(EdgeStopTask, NULL, &threadAttribute) == NULL) {
        printf("[%s][BOOT] TASK_CREATE_FAILED\r\n", TASK_VERSION);
    }
}

APP_FEATURE_INIT(TableEdgeTest);
