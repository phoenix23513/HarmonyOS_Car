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
#define MOTOR_FRAME_HEAD 0xFC
#define MOTOR_FRAME_TAIL 0xFD

/* Scheme 1: the black line runs between the two sensors. */
#define TRACK_LEFT_LEVEL WIFI_IOT_GPIO_VALUE0
#define TRACK_RIGHT_LEVEL WIFI_IOT_GPIO_VALUE0

#define STRAIGHT_LEFT_SPEED 30
#define STRAIGHT_RIGHT_SPEED 29
#define LOOP_LEFT_SPEED 27
#define LOOP_RIGHT_SPEED 26
#define FINISH_LEFT_SPEED 22
#define FINISH_RIGHT_SPEED 21
#define CORRECT_INNER_SPEED 12
#define CORRECT_OUTER_SPEED 32
#define HARD_INNER_SPEED 0
#define HARD_OUTER_SPEED 30
#define MARKER_FORWARD_SPEED 17

/* Junctions use forward-only pivot search; neither wheel ever reverses. */
#define JUNCTION_1_PIVOT_SPEED 25
#define JUNCTION_2_PIVOT_SPEED 24
#define JUNCTION_FINE_INNER_SPEED 8
#define JUNCTION_FINE_OUTER_SPEED 20
#define JUNCTION_COUNTER_INNER_SPEED 8
#define JUNCTION_COUNTER_OUTER_SPEED 18
#define JUNCTION_FORWARD_SPEED 13

#define SENSOR_SAMPLE_TICKS 2U
#define MOTOR_REFRESH_TICKS 10U
#define START_DELAY_TICKS 300U
#define START_CONFIRM_COUNT 10U
#define INPUT_CONFIRM_COUNT 2U
#define HARD_CORRECTION_COUNT 6U
#define CORRECTION_HOLD_COUNT 3U
#define MARKER_CONFIRM_COUNT 5U
#define JUNCTION_INITIAL_PIVOT_TICKS 6U
#define JUNCTION_PIVOT_STEP_TICKS 2U
#define JUNCTION_MAX_PIVOT_TICKS 14U
#define JUNCTION_ESCALATE_COUNT 4U
#define JUNCTION_FINE_TURN_TICKS 5U
#define JUNCTION_COUNTER_TICKS 4U
#define JUNCTION_FORWARD_PROBE_TICKS 6U
#define TRACK_REACQUIRE_COUNT 4U

#define TURN_LEFT 0
#define TURN_RIGHT 1
#define JUNCTION_1_DIRECTION TURN_LEFT
#define JUNCTION_2_DIRECTION TURN_RIGHT
#define TASK_VERSION "FIXED_TRACK_CLAMP_V4"

typedef enum {
    ROUTE_WAIT_START = 0,
    ROUTE_TO_JUNCTION_1,
    ROUTE_ON_OUTER_LOOP,
    ROUTE_TO_FINISH,
    ROUTE_FINISHED,
    ROUTE_ERROR_STOP
} RoutePhase;

typedef enum {
    DRIVE_UNKNOWN = 0,
    DRIVE_STRAIGHT,
    DRIVE_LEFT,
    DRIVE_RIGHT,
    DRIVE_MARKER,
    DRIVE_STOPPED
} DriveState;

static uint8_t g_motorFrame[6];
static DriveState g_lastDriveState = DRIVE_UNKNOWN;

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

    g_motorFrame[0] = MOTOR_FRAME_HEAD;
    g_motorFrame[1] = leftDirection;
    g_motorFrame[2] = (uint8_t)left;
    g_motorFrame[3] = rightDirection;
    g_motorFrame[4] = (uint8_t)right;
    g_motorFrame[5] = MOTOR_FRAME_TAIL;
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

static const char *PhaseText(RoutePhase phase)
{
    switch (phase) {
        case ROUTE_WAIT_START:
            return "WAIT_START";
        case ROUTE_TO_JUNCTION_1:
            return "TO_JUNCTION_1";
        case ROUTE_ON_OUTER_LOOP:
            return "ON_OUTER_LOOP";
        case ROUTE_TO_FINISH:
            return "TO_FINISH";
        case ROUTE_FINISHED:
            return "FINISHED";
        default:
            return "ERROR_STOP";
    }
}

static void SetDrive(DriveState state, int left, int right,
                     RoutePhase phase)
{
    SendMotorCommand(left, right);
    if (state != g_lastDriveState) {
        printf("[%s][DRIVE] PHASE=%s STATE=%u MOTOR=%d/%d\r\n",
               TASK_VERSION, PhaseText(phase), (unsigned int)state,
               left, right);
        g_lastDriveState = state;
    }
}

static int IsCentered(WifiIotGpioValue left,
                      WifiIotGpioValue right)
{
    return left == WIFI_IOT_GPIO_VALUE0 &&
           right == WIFI_IOT_GPIO_VALUE0;
}

static int IsExpectedTurnSide(WifiIotGpioValue left,
                              WifiIotGpioValue right,
                              int turnRight)
{
    if (turnRight) {
        return left == WIFI_IOT_GPIO_VALUE0 &&
               right == WIFI_IOT_GPIO_VALUE1;
    }
    return left == WIFI_IOT_GPIO_VALUE1 &&
           right == WIFI_IOT_GPIO_VALUE0;
}

static void RunMotorForTicks(int leftSpeed, int rightSpeed,
                             uint32_t durationTicks)
{
    SendMotorCommand(leftSpeed, rightSpeed);
    osDelay(durationTicks);
}

static void RunPivotForTicks(int turnRight, int pivotSpeed,
                             uint32_t durationTicks)
{
    if (turnRight) {
        RunMotorForTicks(pivotSpeed, 0, durationTicks);
    } else {
        RunMotorForTicks(0, pivotSpeed, durationTicks);
    }
}

static int IsOppositeTurnSide(WifiIotGpioValue left,
                              WifiIotGpioValue right,
                              int turnRight)
{
    return IsExpectedTurnSide(left, right, !turnRight);
}

static int RunJunctionTurn(int turnRight, unsigned int junctionNumber)
{
    WifiIotGpioValue left = WIFI_IOT_GPIO_VALUE0;
    WifiIotGpioValue right = WIFI_IOT_GPIO_VALUE0;
    uint32_t centeredCount = 0;
    uint32_t blackCount = 0;
    uint32_t pivotTicks = JUNCTION_INITIAL_PIVOT_TICKS;
    int pivotSpeed = junctionNumber == 1U ?
                     JUNCTION_1_PIVOT_SPEED : JUNCTION_2_PIVOT_SPEED;

    printf("[%s][JUNCTION] NUMBER=%u ACTION=%s MODE=PIV_SEARCH\r\n",
           TASK_VERSION, junctionNumber,
           turnRight ? "TURN_RIGHT" : "TURN_LEFT");

    /*
     * The confirmed 11 state is the wide center of the Y junction.
     * While it remains 11, pivot toward the fixed branch without advancing.
     * A direct 11 -> 00 transition is valid on this track and starts a
     * slow forward confirmation; 10/01 are handled as edge corrections.
     */
    while (1) {
        if (!ReadIrSensors(&left, &right)) {
            StopMotor();
            printf("[%s][ERROR] GPIO_READ_FAILED_IN_JUNCTION\r\n",
                   TASK_VERSION);
            return 0;
        }

        if (left == WIFI_IOT_GPIO_VALUE1 &&
            right == WIFI_IOT_GPIO_VALUE1) {
            centeredCount = 0;
            blackCount++;

            if (blackCount % JUNCTION_ESCALATE_COUNT == 0U &&
                pivotTicks < JUNCTION_MAX_PIVOT_TICKS) {
                pivotTicks += JUNCTION_PIVOT_STEP_TICKS;
                if (pivotTicks > JUNCTION_MAX_PIVOT_TICKS) {
                    pivotTicks = JUNCTION_MAX_PIVOT_TICKS;
                }
                printf("[%s][JUNCTION] NUMBER=%u PIVOT_TICKS=%u\r\n",
                       TASK_VERSION, junctionNumber,
                       (unsigned int)pivotTicks);
            }

            RunPivotForTicks(turnRight, pivotSpeed, pivotTicks);
        } else if (IsCentered(left, right)) {
            blackCount = 0;
            centeredCount++;
            RunMotorForTicks(JUNCTION_FORWARD_SPEED,
                             JUNCTION_FORWARD_SPEED,
                             JUNCTION_FORWARD_PROBE_TICKS);

            if (centeredCount >= TRACK_REACQUIRE_COUNT) {
                printf("[%s][JUNCTION] NUMBER=%u TRACK_REACQUIRED\r\n",
                       TASK_VERSION, junctionNumber);
                return 1;
            }
        } else if (IsExpectedTurnSide(left, right, turnRight)) {
            blackCount = 0;
            centeredCount = 0;
            if (turnRight) {
                RunMotorForTicks(JUNCTION_FINE_OUTER_SPEED,
                                 JUNCTION_FINE_INNER_SPEED,
                                 JUNCTION_FINE_TURN_TICKS);
            } else {
                RunMotorForTicks(JUNCTION_FINE_INNER_SPEED,
                                 JUNCTION_FINE_OUTER_SPEED,
                                 JUNCTION_FINE_TURN_TICKS);
            }
        } else if (IsOppositeTurnSide(left, right, turnRight)) {
            blackCount = 0;
            centeredCount = 0;
            if (turnRight) {
                RunMotorForTicks(JUNCTION_COUNTER_INNER_SPEED,
                                 JUNCTION_COUNTER_OUTER_SPEED,
                                 JUNCTION_COUNTER_TICKS);
            } else {
                RunMotorForTicks(JUNCTION_COUNTER_OUTER_SPEED,
                                 JUNCTION_COUNTER_INNER_SPEED,
                                 JUNCTION_COUNTER_TICKS);
            }
        }
    }
}

static void ApplyStraight(RoutePhase phase)
{
    if (phase == ROUTE_ON_OUTER_LOOP) {
        SetDrive(DRIVE_STRAIGHT, LOOP_LEFT_SPEED,
                 LOOP_RIGHT_SPEED, phase);
    } else if (phase == ROUTE_TO_FINISH) {
        SetDrive(DRIVE_STRAIGHT, FINISH_LEFT_SPEED,
                 FINISH_RIGHT_SPEED, phase);
    } else {
        SetDrive(DRIVE_STRAIGHT, STRAIGHT_LEFT_SPEED,
                 STRAIGHT_RIGHT_SPEED, phase);
    }
}

static void ApplyLeftCorrection(uint32_t count, RoutePhase phase)
{
    if (count < HARD_CORRECTION_COUNT) {
        SetDrive(DRIVE_LEFT, CORRECT_INNER_SPEED,
                 CORRECT_OUTER_SPEED, phase);
    } else {
        SetDrive(DRIVE_LEFT, HARD_INNER_SPEED,
                 HARD_OUTER_SPEED, phase);
    }
}

static void ApplyRightCorrection(uint32_t count, RoutePhase phase)
{
    if (count < HARD_CORRECTION_COUNT) {
        SetDrive(DRIVE_RIGHT, CORRECT_OUTER_SPEED,
                 CORRECT_INNER_SPEED, phase);
    } else {
        SetDrive(DRIVE_RIGHT, HARD_OUTER_SPEED,
                 HARD_INNER_SPEED, phase);
    }
}

static void LineFollowingTask(void *argument)
{
    WifiIotGpioValue left = WIFI_IOT_GPIO_VALUE0;
    WifiIotGpioValue right = WIFI_IOT_GPIO_VALUE0;
    RoutePhase phase = ROUTE_WAIT_START;
    uint32_t startCount = 0;
    uint32_t leftCount = 0;
    uint32_t rightCount = 0;
    uint32_t bothBlackCount = 0;
    uint32_t correctionHold = 0;
    uint32_t motorRefresh = MOTOR_REFRESH_TICKS;
    int lastCorrection = 0;

    (void)argument;
    printf("[%s][BOOT] SCHEME1_LINE_BETWEEN_SENSORS\r\n",
           TASK_VERSION);
    printf("[%s][INFO] BLACK=1 FLOOR=0 CENTER=00 LEFT=10 RIGHT=01\r\n",
           TASK_VERSION);
    StopMotor();

    while (phase == ROUTE_WAIT_START) {
        if (!ReadIrSensors(&left, &right)) {
            StopMotor();
            printf("[%s][ERROR] GPIO_READ_FAILED_AT_START\r\n",
                   TASK_VERSION);
            osDelay(SENSOR_SAMPLE_TICKS);
            continue;
        }
        if (left == TRACK_LEFT_LEVEL && right == TRACK_RIGHT_LEVEL) {
            startCount++;
        } else {
            startCount = 0;
        }
        if (startCount >= START_CONFIRM_COUNT) {
            printf("[%s][START] CENTER_00_OK WAITING\r\n", TASK_VERSION);
            osDelay(START_DELAY_TICKS);
            if (ReadIrSensors(&left, &right) && IsCentered(left, right)) {
                phase = ROUTE_TO_JUNCTION_1;
                printf("[%s][PHASE] TO_JUNCTION_1\r\n", TASK_VERSION);
                break;
            }
            startCount = 0;
            printf("[%s][START] POSITION_CHANGED WAIT_FOR_00\r\n",
                   TASK_VERSION);
        }
        osDelay(SENSOR_SAMPLE_TICKS);
    }

    while (phase != ROUTE_FINISHED && phase != ROUTE_ERROR_STOP) {
        if (!ReadIrSensors(&left, &right)) {
            StopMotor();
            phase = ROUTE_ERROR_STOP;
            printf("[%s][ERROR] GPIO_READ_FAILED STOPPED\r\n",
                   TASK_VERSION);
            break;
        }

        if (left == WIFI_IOT_GPIO_VALUE0 &&
            right == WIFI_IOT_GPIO_VALUE0) {
            leftCount = 0;
            rightCount = 0;
            bothBlackCount = 0;
            if (correctionHold > 0U) {
                correctionHold--;
                if (lastCorrection < 0) {
                    ApplyLeftCorrection(1U, phase);
                } else if (lastCorrection > 0) {
                    ApplyRightCorrection(1U, phase);
                }
            } else if (motorRefresh >= MOTOR_REFRESH_TICKS) {
                ApplyStraight(phase);
                motorRefresh = 0;
            }
        } else if (left == WIFI_IOT_GPIO_VALUE1 &&
                   right == WIFI_IOT_GPIO_VALUE0) {
            leftCount++;
            rightCount = 0;
            bothBlackCount = 0;
            if (leftCount >= INPUT_CONFIRM_COUNT) {
                lastCorrection = -1;
                correctionHold = CORRECTION_HOLD_COUNT;
                ApplyLeftCorrection(leftCount, phase);
            }
        } else if (left == WIFI_IOT_GPIO_VALUE0 &&
                   right == WIFI_IOT_GPIO_VALUE1) {
            rightCount++;
            leftCount = 0;
            bothBlackCount = 0;
            if (rightCount >= INPUT_CONFIRM_COUNT) {
                lastCorrection = 1;
                correctionHold = CORRECTION_HOLD_COUNT;
                ApplyRightCorrection(rightCount, phase);
            }
        } else {
            leftCount = 0;
            rightCount = 0;
            correctionHold = 0;
            bothBlackCount++;
            SetDrive(DRIVE_MARKER, MARKER_FORWARD_SPEED,
                     MARKER_FORWARD_SPEED, phase);

            if (bothBlackCount >= MARKER_CONFIRM_COUNT) {
                if (phase == ROUTE_TO_JUNCTION_1) {
                    if (RunJunctionTurn(JUNCTION_1_DIRECTION, 1U)) {
                        phase = ROUTE_ON_OUTER_LOOP;
                        printf("[%s][PHASE] ON_OUTER_LOOP\r\n",
                               TASK_VERSION);
                    } else {
                        phase = ROUTE_ERROR_STOP;
                    }
                } else if (phase == ROUTE_ON_OUTER_LOOP) {
                    if (RunJunctionTurn(JUNCTION_2_DIRECTION, 2U)) {
                        phase = ROUTE_TO_FINISH;
                        printf("[%s][PHASE] TO_FINISH\r\n",
                               TASK_VERSION);
                    } else {
                        phase = ROUTE_ERROR_STOP;
                    }
                } else {
                    StopMotor();
                    phase = ROUTE_FINISHED;
                    printf("[%s][FINISH] GREEN_FINISH_LINE STOPPED\r\n",
                           TASK_VERSION);
                }
                bothBlackCount = 0;
                leftCount = 0;
                rightCount = 0;
                correctionHold = 0;
                g_lastDriveState = DRIVE_UNKNOWN;
                motorRefresh = MOTOR_REFRESH_TICKS;
            }
        }

        osDelay(SENSOR_SAMPLE_TICKS);
        motorRefresh += SENSOR_SAMPLE_TICKS;
    }

    StopMotor();
    g_lastDriveState = DRIVE_STOPPED;
    while (1) {
        StopMotor();
        osDelay(MOTOR_REFRESH_TICKS);
    }
}

static void LineFollowing(void)
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
    GpioSetDir(IR_LEFT_GPIO, WIFI_IOT_GPIO_DIR_IN);
    GpioSetDir(IR_RIGHT_GPIO, WIFI_IOT_GPIO_DIR_IN);

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11,
              WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12,
              WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);
    UartInit(WIFI_IOT_UART_IDX_2, &uartAttribute, NULL);

    threadAttribute.name = "line_following";
    threadAttribute.stack_size = 3072U;
    threadAttribute.priority = osPriorityNormal;
    if (osThreadNew(LineFollowingTask, NULL, &threadAttribute) == NULL) {
        StopMotor();
        printf("[%s][BOOT] TASK_CREATE_FAILED\r\n", TASK_VERSION);
    }
}

APP_FEATURE_INIT(LineFollowing);
