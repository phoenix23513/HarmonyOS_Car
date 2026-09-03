#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "hi_time.h"
#include "ohos_init.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"
#include "wifiiot_watchdog.h"

#define SERVO_GPIO WIFI_IOT_IO_NAME_GPIO_2
#define TRIG_GPIO WIFI_IOT_IO_NAME_GPIO_7
#define ECHO_GPIO WIFI_IOT_IO_NAME_GPIO_8
#define IR_LEFT_GPIO WIFI_IOT_IO_NAME_GPIO_13
#define IR_RIGHT_GPIO WIFI_IOT_IO_NAME_GPIO_14

#define FRAME_HEAD 0xFC
#define FRAME_TAIL 0xFD
#define FORWARD_LEFT_SPEED 50
#define FORWARD_RIGHT_SPEED 48
#define PATROL_LEFT_SPEED 43
#define PATROL_RIGHT_SPEED 41
#define REVERSE_SPEED 35
#define TURN_SPEED 40

#define SERVO_PERIOD_US 20000U
#define SERVO_LEFT_REAR_US 3075U
#define SERVO_LEFT_SIDE_US 2550U
#define SERVO_LEFT_FRONT_US 2100U
#define SERVO_CENTER_US 1500U
#define SERVO_RIGHT_FRONT_US 1000U
#define SERVO_RIGHT_SIDE_US 600U
#define SERVO_RIGHT_REAR_US 150U
#define SERVO_MOVE_PULSES 8U
#define SERVO_CENTER_PULSES 12U
#define SERVO_SETTLE_TICKS 8U

#define HCSR04_TRIGGER_US 20U
#define HCSR04_TIMEOUT_US 30000U
#define INVALID_DISTANCE_CM (-1.0f)

#define FRONT_AVOID_DISTANCE_CM 16.0f
#define FRONT_EMERGENCY_DISTANCE_CM 14.0f
#define DIAGONAL_AVOID_DISTANCE_CM 20.0f
#define FRONT_RESUME_DISTANCE_CM 16.0f
#define DIAGONAL_RESUME_DISTANCE_CM 15.0f
#define TURN_PATH_CLEAR_DISTANCE_CM 20.0f
#define REAR_REVERSE_CLEAR_DISTANCE_CM 20.0f
#define TURN_TIE_DISTANCE_CM 5.0f
#define REVERSE_DISTANCE_CM 5.0f
#define STUCK_CHANGE_DISTANCE_CM 2.0f

#define START_DELAY_TICKS 300U
#define LOOP_DELAY_TICKS 6U
#define MOTOR_REFRESH_TICKS 10U
#define STOP_SETTLE_TICKS 5U
#define TURN_SEGMENT_TICKS 70U
#define REVERSE_MAX_TICKS 60U
#define BLOCKED_RESCAN_TICKS 100U
#define PATROL_INTERVAL_CYCLES 4U
#define MAX_TURN_SEGMENTS 6U
#define MAX_REVERSE_ATTEMPTS 1U
#define STUCK_CONFIRM_COUNT 2U
#define IR_CONFIRM_SAMPLES 5U
#define IR_CONFIRM_REQUIRED 3U
#define IR_SAFE_CONFIRM_COUNT 5U
#define IR_REVERSE_MAX_TICKS 200U
#define IR_EXTRA_REVERSE_TICKS 80U
#define TASK_VERSION "IR_ULTRASONIC_AVOID_V1"

typedef struct {
    float leftRear;
    float leftSide;
    float leftFront;
    float front;
    float rightFront;
    float rightSide;
    float rightRear;
} SurroundingScan;

static uint8_t g_motorFrame[6];
static uint32_t g_servoPulseUs = SERVO_CENTER_US;
static int g_tieTurnRight = 1;
static int g_forwardHistory = 0;
static int g_irBoundaryPending = 0;

static float AbsFloat(float value)
{
    return value < 0.0f ? -value : value;
}

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

static int IrBoundaryDetected(void)
{
    WifiIotGpioValue left;
    WifiIotGpioValue right;

    if (!ReadIrSensors(&left, &right)) {
        StopMotor();
        g_irBoundaryPending = 1;
        printf("[%s][IR_ERROR] GPIO_READ_FAILED\r\n", TASK_VERSION);
        return 1;
    }
    if (left == WIFI_IOT_GPIO_VALUE1 ||
        right == WIFI_IOT_GPIO_VALUE1) {
        StopMotor();
        g_irBoundaryPending = 1;
        return 1;
    }
    return 0;
}

static int RunMotorFor(int left, int right, uint32_t durationTicks)
{
    uint32_t elapsed = 0;

    while (elapsed < durationTicks) {
        SendMotorCommand(left, right);
        osDelay(MOTOR_REFRESH_TICKS);
        elapsed += MOTOR_REFRESH_TICKS;
        if (IrBoundaryDetected()) {
            return 0;
        }
    }
    return 1;
}

static void ServoPulse(uint32_t pulseUs)
{
    GpioSetOutputVal(SERVO_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(pulseUs);
    GpioSetOutputVal(SERVO_GPIO, WIFI_IOT_GPIO_VALUE0);
    hi_udelay(SERVO_PERIOD_US - pulseUs);
}

static void ServoMoveTo(uint32_t pulseUs)
{
    uint32_t i;
    uint32_t pulseCount = pulseUs == SERVO_CENTER_US ?
                          SERVO_CENTER_PULSES : SERVO_MOVE_PULSES;

    for (i = 0; i < pulseCount; i++) {
        ServoPulse(pulseUs);
        if (IrBoundaryDetected()) {
            break;
        }
    }
    g_servoPulseUs = pulseUs;
    osDelay(SERVO_SETTLE_TICKS);
}

static void ServoCenter(void)
{
    if (g_servoPulseUs != SERVO_CENTER_US) {
        ServoMoveTo(SERVO_CENTER_US);
    }
}

static int WaitEcho(WifiIotGpioValue expected, uint32_t timeoutUs)
{
    uint32_t start = hi_get_us();
    WifiIotGpioValue value = WIFI_IOT_GPIO_VALUE0;

    while ((uint32_t)(hi_get_us() - start) < timeoutUs) {
        if (GpioGetInputVal(ECHO_GPIO, &value) != WIFI_IOT_SUCCESS) {
            return 0;
        }
        if (value == expected) {
            return 1;
        }
    }
    return 0;
}

static float ReadDistance(void)
{
    uint32_t echoStart;
    uint32_t echoTime;

    if (!WaitEcho(WIFI_IOT_GPIO_VALUE0, HCSR04_TIMEOUT_US)) {
        return INVALID_DISTANCE_CM;
    }

    GpioSetOutputVal(TRIG_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(HCSR04_TRIGGER_US);
    GpioSetOutputVal(TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);

    if (!WaitEcho(WIFI_IOT_GPIO_VALUE1, HCSR04_TIMEOUT_US)) {
        return INVALID_DISTANCE_CM;
    }
    echoStart = hi_get_us();
    if (!WaitEcho(WIFI_IOT_GPIO_VALUE0, HCSR04_TIMEOUT_US)) {
        return INVALID_DISTANCE_CM;
    }

    echoTime = (uint32_t)(hi_get_us() - echoStart);
    if (echoTime == 0U || echoTime >= HCSR04_TIMEOUT_US) {
        return INVALID_DISTANCE_CM;
    }
    return echoTime * 0.034f / 2.0f;
}

static float ReadQuickDistance(void)
{
    float first = ReadDistance();
    float second;

    osDelay(LOOP_DELAY_TICKS);
    second = ReadDistance();
    if (first < 0.0f) {
        return second;
    }
    if (second < 0.0f) {
        return first;
    }
    return (first + second) / 2.0f;
}

static float MeasureAt(uint32_t pulseUs)
{
    ServoMoveTo(pulseUs);
    return ReadQuickDistance();
}

static SurroundingScan ScanSurroundings(void)
{
    SurroundingScan scan;

    StopMotor();
    ServoMoveTo(SERVO_CENTER_US);
    scan.front = ReadQuickDistance();
    scan.leftFront = MeasureAt(SERVO_LEFT_FRONT_US);
    scan.leftSide = MeasureAt(SERVO_LEFT_SIDE_US);
    scan.leftRear = MeasureAt(SERVO_LEFT_REAR_US);
    ServoMoveTo(SERVO_CENTER_US);
    scan.rightFront = MeasureAt(SERVO_RIGHT_FRONT_US);
    scan.rightSide = MeasureAt(SERVO_RIGHT_SIDE_US);
    scan.rightRear = MeasureAt(SERVO_RIGHT_REAR_US);
    ServoMoveTo(SERVO_CENTER_US);
    scan.front = ReadQuickDistance();

    printf("[%s][FULL_SCAN] LR=%.1f LS=%.1f LF=%.1f F=%.1f "
           "RF=%.1f RS=%.1f RR=%.1f\r\n",
           TASK_VERSION, scan.leftRear, scan.leftSide,
           scan.leftFront, scan.front, scan.rightFront,
           scan.rightSide, scan.rightRear);
    return scan;
}

static float DirectionScore(float rear, float side, float front)
{
    float score = 10000.0f;
    uint32_t validCount = 0;

    if (rear >= 0.0f) {
        score = rear;
        validCount++;
    }
    if (side >= 0.0f) {
        if (side < score) {
            score = side;
        }
        validCount++;
    }
    if (front >= 0.0f) {
        if (front < score) {
            score = front;
        }
        validCount++;
    }
    return validCount >= 2U ? score : INVALID_DISTANCE_CM;
}

static int EscapeReady(const SurroundingScan *scan)
{
    return scan->front >= FRONT_RESUME_DISTANCE_CM &&
           scan->leftFront >= DIAGONAL_RESUME_DISTANCE_CM &&
           scan->rightFront >= DIAGONAL_RESUME_DISTANCE_CM;
}

static int ChooseTurn(const SurroundingScan *scan, int *turnRight)
{
    float leftScore = DirectionScore(scan->leftRear, scan->leftSide,
                                     scan->leftFront);
    float rightScore = DirectionScore(scan->rightRear, scan->rightSide,
                                      scan->rightFront);
    int leftClear = leftScore >= TURN_PATH_CLEAR_DISTANCE_CM;
    int rightClear = rightScore >= TURN_PATH_CLEAR_DISTANCE_CM;

    if (!leftClear && !rightClear) {
        return 0;
    }
    if (leftClear && !rightClear) {
        *turnRight = 0;
    } else if (!leftClear && rightClear) {
        *turnRight = 1;
    } else if (leftScore - rightScore > TURN_TIE_DISTANCE_CM) {
        *turnRight = 0;
    } else if (rightScore - leftScore > TURN_TIE_DISTANCE_CM) {
        *turnRight = 1;
    } else {
        *turnRight = g_tieTurnRight;
        g_tieTurnRight = !g_tieTurnRight;
    }

    printf("[%s][CHOICE] %s LEFT_SCORE=%.1f RIGHT_SCORE=%.1f\r\n",
           TASK_VERSION, *turnRight ? "RIGHT" : "LEFT",
           leftScore, rightScore);
    return 1;
}

static int ScanUnchanged(const SurroundingScan *before,
                         const SurroundingScan *after)
{
    const float *oldValues = (const float *)before;
    const float *newValues = (const float *)after;
    uint32_t comparable = 0;
    uint32_t unchanged = 0;
    uint32_t i;

    for (i = 0; i < 7U; i++) {
        if (oldValues[i] >= 0.0f && newValues[i] >= 0.0f) {
            comparable++;
            if (AbsFloat(oldValues[i] - newValues[i]) <
                STUCK_CHANGE_DISTANCE_CM) {
                unchanged++;
            }
        }
    }
    return comparable >= 5U && unchanged == comparable;
}

static int ReverseAllowed(const SurroundingScan *scan)
{
    return g_forwardHistory &&
           scan->leftRear >= REAR_REVERSE_CLEAR_DISTANCE_CM &&
           scan->rightRear >= REAR_REVERSE_CLEAR_DISTANCE_CM;
}

static int ReverseLimited(const SurroundingScan *scan)
{
    float startDistance;
    float currentDistance;
    uint32_t elapsed = 0;

    if (!ReverseAllowed(scan)) {
        printf("[%s][REVERSE] NOT_ALLOWED LR=%.1f RR=%.1f\r\n",
               TASK_VERSION, scan->leftRear, scan->rightRear);
        return 0;
    }

    ServoCenter();
    startDistance = ReadQuickDistance();
    if (startDistance < 0.0f) {
        return 0;
    }

    printf("[%s][ACTION] REVERSE START=%.1fcm\r\n",
           TASK_VERSION, startDistance);
    currentDistance = startDistance;
    while (elapsed < REVERSE_MAX_TICKS &&
           currentDistance - startDistance < REVERSE_DISTANCE_CM) {
        SendMotorCommand(-REVERSE_SPEED, -REVERSE_SPEED);
        osDelay(MOTOR_REFRESH_TICKS);
        elapsed += MOTOR_REFRESH_TICKS;
        currentDistance = ReadDistance();
        if (currentDistance < 0.0f) {
            StopMotor();
            printf("[%s][REVERSE] DIST_ERR STOPPED\r\n", TASK_VERSION);
            return 0;
        }
    }
    StopMotor();
    printf("[%s][ACTION] REVERSE_DONE DIST=%.1fcm\r\n",
           TASK_VERSION, currentDistance);
    return 1;
}

static int TurnSegment(int turnRight)
{
    int left = turnRight ? TURN_SPEED : -TURN_SPEED;
    int right = turnRight ? -TURN_SPEED : TURN_SPEED;

    printf("[%s][ACTION] TURN_%s\r\n", TASK_VERSION,
           turnRight ? "RIGHT" : "LEFT");
    if (!RunMotorFor(left, right, TURN_SEGMENT_TICKS)) {
        StopMotor();
        printf("[%s][IR_BOUNDARY] TURN_ABORTED\r\n", TASK_VERSION);
        return 0;
    }
    StopMotor();
    osDelay(STOP_SETTLE_TICKS);
    return 1;
}

static int AvoidObstacle(int emergency, int forceTurn)
{
    SurroundingScan before;
    SurroundingScan after;
    uint32_t turnSegments = 0;
    uint32_t reverseAttempts = 0;
    uint32_t stuckCount = 0;
    int turnRight;

    StopMotor();
    before = ScanSurroundings();
    while (turnSegments < MAX_TURN_SEGMENTS) {
        if (!forceTurn && EscapeReady(&before)) {
            printf("[%s][AVOID] PATH_CLEAR\r\n", TASK_VERSION);
            return 1;
        }

        /* Too close to rotate safely: make one slow, strictly limited retreat. */
        if ((emergency ||
             (before.front >= 0.0f &&
              before.front < FRONT_EMERGENCY_DISTANCE_CM)) &&
            reverseAttempts < MAX_REVERSE_ATTEMPTS &&
            ReverseLimited(&before)) {
            reverseAttempts++;
            emergency = 0;
            before = ScanSurroundings();
            continue;
        }

        /* A single wall normally leaves a clear side, so turn before reversing. */
        if (!ChooseTurn(&before, &turnRight)) {
            if (reverseAttempts >= MAX_REVERSE_ATTEMPTS ||
                !ReverseLimited(&before)) {
                printf("[%s][BLOCKED] NO_SAFE_TURN\r\n", TASK_VERSION);
                return 0;
            }
            reverseAttempts++;
            before = ScanSurroundings();
            continue;
        }

        if (!TurnSegment(turnRight)) {
            return 0;
        }
        forceTurn = 0;
        after = ScanSurroundings();
        turnSegments++;

        if (ScanUnchanged(&before, &after)) {
            stuckCount++;
            printf("[%s][STUCK_CHECK] COUNT=%u\r\n", TASK_VERSION,
                   (unsigned int)stuckCount);
        } else {
            stuckCount = 0;
        }

        if (EscapeReady(&after)) {
            printf("[%s][AVOID] CLEAR_AFTER_SCAN\r\n", TASK_VERSION);
            return 1;
        }

        if (stuckCount >= STUCK_CONFIRM_COUNT) {
            printf("[%s][STUCK] TURN_NO_POSITION_CHANGE\r\n",
                   TASK_VERSION);
            if (reverseAttempts >= MAX_REVERSE_ATTEMPTS ||
                !ReverseLimited(&after)) {
                return 0;
            }
            reverseAttempts++;
            after = ScanSurroundings();
            stuckCount = 0;
        }
        before = after;
    }

    printf("[%s][BLOCKED] MAX_TURN_SEGMENTS\r\n", TASK_VERSION);
    return 0;
}

static int ConfirmIrBoundary(void)
{
    WifiIotGpioValue left;
    WifiIotGpioValue right;
    uint32_t detected = 0;
    uint32_t i;

    StopMotor();
    for (i = 0; i < IR_CONFIRM_SAMPLES; i++) {
        if (!ReadIrSensors(&left, &right)) {
            return -1;
        }
        if (left == WIFI_IOT_GPIO_VALUE1 ||
            right == WIFI_IOT_GPIO_VALUE1) {
            detected++;
        }
        osDelay(LOOP_DELAY_TICKS);
    }
    return detected >= IR_CONFIRM_REQUIRED;
}

static int ReverseFromIrBoundary(void)
{
    WifiIotGpioValue left;
    WifiIotGpioValue right;
    uint32_t elapsed = 0;
    uint32_t refresh = MOTOR_REFRESH_TICKS;
    uint32_t safeCount = 0;

    printf("[%s][IR_ACTION] REVERSE_ONLY SPEED=%d\r\n",
           TASK_VERSION, REVERSE_SPEED);
    while (elapsed < IR_REVERSE_MAX_TICKS) {
        if (refresh >= MOTOR_REFRESH_TICKS) {
            SendMotorCommand(-REVERSE_SPEED, -REVERSE_SPEED);
            refresh = 0;
        }
        if (!ReadIrSensors(&left, &right)) {
            StopMotor();
            printf("[%s][IR_ERROR] GPIO_READ_FAILED STOPPED\r\n",
                   TASK_VERSION);
            return 0;
        }
        if (left == WIFI_IOT_GPIO_VALUE0 &&
            right == WIFI_IOT_GPIO_VALUE0) {
            safeCount++;
        } else {
            safeCount = 0;
        }
        if (safeCount >= IR_SAFE_CONFIRM_COUNT) {
            uint32_t extra = 0;

            printf("[%s][IR_SAFE] BLACK_CLEARED EXTRA_REVERSE=%u\r\n",
                   TASK_VERSION, IR_EXTRA_REVERSE_TICKS);
            while (extra < IR_EXTRA_REVERSE_TICKS) {
                SendMotorCommand(-REVERSE_SPEED, -REVERSE_SPEED);
                osDelay(MOTOR_REFRESH_TICKS);
                extra += MOTOR_REFRESH_TICKS;
            }
            StopMotor();
            return 1;
        }
        osDelay(LOOP_DELAY_TICKS);
        elapsed += LOOP_DELAY_TICKS;
        refresh += LOOP_DELAY_TICKS;
    }

    StopMotor();
    printf("[%s][IR_ERROR] REVERSE_TIMEOUT STOPPED\r\n", TASK_VERSION);
    return 0;
}

static int HandleIrBoundary(void)
{
    int confirmed;

    StopMotor();
    confirmed = ConfirmIrBoundary();
    if (confirmed < 0) {
        printf("[%s][IR_ERROR] CONFIRM_READ_FAILED\r\n", TASK_VERSION);
        return 0;
    }
    if (!confirmed) {
        g_irBoundaryPending = 0;
        printf("[%s][IR] FALSE_TRIGGER\r\n", TASK_VERSION);
        return 1;
    }

    printf("[%s][IR_BOUNDARY] BLACK_CONFIRMED\r\n", TASK_VERSION);
    if (!ReverseFromIrBoundary()) {
        return 0;
    }
    g_irBoundaryPending = 0;
    printf("[%s][IR_ACTION] CLEAR_THEN_ULTRASONIC_TURN\r\n",
           TASK_VERSION);
    return AvoidObstacle(0, 1);
}

static int PatrolDiagonal(int lookLeft, float *distance)
{
    uint32_t pulseUs = lookLeft ? SERVO_LEFT_FRONT_US :
                                  SERVO_RIGHT_FRONT_US;

    SendMotorCommand(PATROL_LEFT_SPEED, PATROL_RIGHT_SPEED);
    *distance = MeasureAt(pulseUs);
    ServoMoveTo(SERVO_CENTER_US);
    if (g_irBoundaryPending) {
        StopMotor();
        return 0;
    }
    SendMotorCommand(FORWARD_LEFT_SPEED, FORWARD_RIGHT_SPEED);

    printf("[%s][PATROL] %s_FRONT=%.1fcm\r\n", TASK_VERSION,
           lookLeft ? "LEFT" : "RIGHT", *distance);
    return *distance >= 0.0f &&
           *distance >= DIAGONAL_AVOID_DISTANCE_CM;
}

static int PatrolSceneUnchanged(float oldLeft, float oldFront,
                                float oldRight, float newLeft,
                                float newFront, float newRight)
{
    if (oldLeft < 0.0f || oldFront < 0.0f || oldRight < 0.0f ||
        newLeft < 0.0f || newFront < 0.0f || newRight < 0.0f) {
        return 0;
    }
    return AbsFloat(oldLeft - newLeft) < STUCK_CHANGE_DISTANCE_CM &&
           AbsFloat(oldFront - newFront) < STUCK_CHANGE_DISTANCE_CM &&
           AbsFloat(oldRight - newRight) < STUCK_CHANGE_DISTANCE_CM;
}

static void AvoidanceTask(void *argument)
{
    float leftFront = INVALID_DISTANCE_CM;
    float rightFront = INVALID_DISTANCE_CM;
    float oldLeft = INVALID_DISTANCE_CM;
    float oldFront = INVALID_DISTANCE_CM;
    float oldRight = INVALID_DISTANCE_CM;
    uint32_t nearCount = 0;
    uint32_t patrolCycles = 0;
    uint32_t stuckCount = 0;
    int lookLeft = 1;
    int blocked = 0;

    (void)argument;
    printf("[%s][BOOT] ACTIVE_SCAN_ULTRASONIC_AVOIDANCE\r\n",
           TASK_VERSION);
    StopMotor();
    ServoMoveTo(SERVO_CENTER_US);
    osDelay(START_DELAY_TICKS);

    while (1) {
        float front;

        if (g_irBoundaryPending || IrBoundaryDetected()) {
            if (!HandleIrBoundary()) {
                blocked = 1;
            }
            nearCount = 0;
            continue;
        }

        if (blocked) {
            SurroundingScan scan = ScanSurroundings();

            if (EscapeReady(&scan)) {
                blocked = 0;
                stuckCount = 0;
                printf("[%s][BLOCKED] ENVIRONMENT_CLEAR\r\n",
                       TASK_VERSION);
            } else {
                StopMotor();
                osDelay(BLOCKED_RESCAN_TICKS);
                continue;
            }
        }

        ServoCenter();
        front = ReadDistance();
        if (g_irBoundaryPending || IrBoundaryDetected()) {
            if (!HandleIrBoundary()) {
                blocked = 1;
            }
            nearCount = 0;
            continue;
        }
        if (front < 0.0f) {
            StopMotor();
            printf("[%s][ERROR] FRONT_DIST_ERR\r\n", TASK_VERSION);
            if (!AvoidObstacle(0, 0) && !g_irBoundaryPending) {
                blocked = 1;
            }
            continue;
        }

        if (front < FRONT_EMERGENCY_DISTANCE_CM) {
            nearCount = 2U;
        } else if (front < FRONT_AVOID_DISTANCE_CM) {
            nearCount++;
        } else {
            nearCount = 0;
        }

        if (nearCount >= 2U) {
            StopMotor();
            printf("[%s][OBSTACLE] FRONT=%.1fcm\r\n",
                   TASK_VERSION, front);
            if (!AvoidObstacle(front < FRONT_EMERGENCY_DISTANCE_CM, 0) &&
                !g_irBoundaryPending) {
                blocked = 1;
            }
            nearCount = 0;
            continue;
        }

        SendMotorCommand(FORWARD_LEFT_SPEED, FORWARD_RIGHT_SPEED);
        g_forwardHistory = 1;
        patrolCycles++;

        if (patrolCycles >= PATROL_INTERVAL_CYCLES) {
            float diagonal;

            patrolCycles = 0;
            if (!PatrolDiagonal(lookLeft, &diagonal)) {
                StopMotor();
                printf("[%s][OBSTACLE] %s_DIAGONAL=%.1fcm\r\n",
                       TASK_VERSION, lookLeft ? "LEFT" : "RIGHT",
                       diagonal);
                if (!g_irBoundaryPending && !AvoidObstacle(0, 0)) {
                    blocked = 1;
                }
                nearCount = 0;
                continue;
            }

            if (lookLeft) {
                leftFront = diagonal;
            } else {
                rightFront = diagonal;
            }
            lookLeft = !lookLeft;

            if (leftFront >= 0.0f && rightFront >= 0.0f) {
                if (PatrolSceneUnchanged(oldLeft, oldFront, oldRight,
                                         leftFront, front, rightFront)) {
                    stuckCount++;
                } else {
                    stuckCount = 0;
                }
                oldLeft = leftFront;
                oldFront = front;
                oldRight = rightFront;

                if (stuckCount >= STUCK_CONFIRM_COUNT) {
                    StopMotor();
                    printf("[%s][STUCK] FORWARD_NO_POSITION_CHANGE\r\n",
                           TASK_VERSION);
                    if (!AvoidObstacle(0, 0) && !g_irBoundaryPending) {
                        blocked = 1;
                    }
                    stuckCount = 0;
                    continue;
                }
            }
        }

        printf("[%s][RUN] FRONT=%.1fcm SPEED=%d/%d\r\n",
               TASK_VERSION, front, FORWARD_LEFT_SPEED,
               FORWARD_RIGHT_SPEED);
        osDelay(LOOP_DELAY_TICKS);
    }
}

static void UltrasonicAvoidance(void)
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

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_2, WIFI_IOT_IO_FUNC_GPIO_2_GPIO);
    GpioSetDir(SERVO_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetOutputVal(SERVO_GPIO, WIFI_IOT_GPIO_VALUE0);

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_7, WIFI_IOT_IO_FUNC_GPIO_7_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_8, WIFI_IOT_IO_FUNC_GPIO_8_GPIO);
    GpioSetDir(TRIG_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetDir(ECHO_GPIO, WIFI_IOT_GPIO_DIR_IN);
    GpioSetOutputVal(TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);

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

    attr.name = "ultrasonic_avoidance";
    attr.stack_size = 8192U;
    attr.priority = osPriorityNormal;
    if (osThreadNew(AvoidanceTask, NULL, &attr) == NULL) {
        printf("[%s][BOOT] TASK_CREATE_FAILED\r\n", TASK_VERSION);
    }
}

APP_FEATURE_INIT(UltrasonicAvoidance);
