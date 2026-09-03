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
#define FRAME_HEAD 0xFC
#define FRAME_TAIL 0xFD
#define FORWARD_LEFT 40
#define FORWARD_RIGHT 38
#define TURN_SPEED 40
#define SERVO_PERIOD_US 20000U
#define SERVO_CENTER_US 1500U
#define SERVO_LEFT_MID_US 2100U
#define SERVO_LEFT_US 2550U
#define SERVO_RIGHT_MID_US 1000U
#define SERVO_RIGHT_US 600U
#define SERVO_TRANSITION_PULSES 8U
#define SERVO_POSITION_PULSES 15U
#define SERVO_SETTLE_TICKS 30U
#define HCSR04_TRIGGER_US 20U
#define HCSR04_TIMEOUT_US 30000U
#define INVALID_DISTANCE_CM (-1.0f)
#define AVOID_DISTANCE_CM 18.0f
#define EMERGENCY_DISTANCE_CM 15.0f
#define TURN_CLEAR_DISTANCE_CM 25.0f
#define RESUME_DISTANCE_CM 25.0f
#define TURN_TIE_CM 5.0f
#define START_DELAY_TICKS 300U
#define LOOP_DELAY_TICKS 6U
#define STOP_WAIT_TICKS 10U
#define MOTOR_REFRESH_TICKS 10U
#define TURN_CHUNK_TICKS 70U
#define TURN_SETTLE_TICKS 5U
#define MAX_TURN_TICKS 210U
#define BLOCKED_RETRY_TICKS 50U
#define TASK_VERSION "ULTRASONIC_AVOID_V2"

static uint8_t g_frame[6];
static int g_tieTurnRight = 1;

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
    g_frame[0] = FRAME_HEAD;
    g_frame[1] = leftDirection;
    g_frame[2] = (uint8_t)left;
    g_frame[3] = rightDirection;
    g_frame[4] = (uint8_t)right;
    g_frame[5] = FRAME_TAIL;
    UartWrite(WIFI_IOT_UART_IDX_2, g_frame, sizeof(g_frame));
}

static void RunMotorFor(int left, int right, uint32_t durationTicks)
{
    uint32_t elapsed = 0;

    while (elapsed < durationTicks) {
        SendMotorCommand(left, right);
        osDelay(MOTOR_REFRESH_TICKS);
        elapsed += MOTOR_REFRESH_TICKS;
    }
}

static void ServoPulse(uint32_t pulseUs)
{
    GpioSetOutputVal(SERVO_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(pulseUs);
    GpioSetOutputVal(SERVO_GPIO, WIFI_IOT_GPIO_VALUE0);
    hi_udelay(SERVO_PERIOD_US - pulseUs);
}

static void ServoMove(uint32_t pulseUs, uint32_t count)
{
    uint32_t i;

    for (i = 0; i < count; i++) {
        ServoPulse(pulseUs);
    }
}

static void ServoCenter(void)
{
    ServoMove(SERVO_CENTER_US, SERVO_POSITION_PULSES);
}

static void ServoLeft(void)
{
    ServoMove(SERVO_LEFT_MID_US, SERVO_TRANSITION_PULSES);
    ServoMove(SERVO_LEFT_US, SERVO_POSITION_PULSES);
}

static void ServoRight(void)
{
    ServoMove(SERVO_RIGHT_MID_US, SERVO_TRANSITION_PULSES);
    ServoMove(SERVO_RIGHT_US, SERVO_POSITION_PULSES);
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
    uint32_t start;
    uint32_t echoUs;

    if (!WaitEcho(WIFI_IOT_GPIO_VALUE0, HCSR04_TIMEOUT_US)) {
        return INVALID_DISTANCE_CM;
    }
    GpioSetOutputVal(TRIG_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(HCSR04_TRIGGER_US);
    GpioSetOutputVal(TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);
    if (!WaitEcho(WIFI_IOT_GPIO_VALUE1, HCSR04_TIMEOUT_US)) {
        return INVALID_DISTANCE_CM;
    }
    start = hi_get_us();
    if (!WaitEcho(WIFI_IOT_GPIO_VALUE0, HCSR04_TIMEOUT_US)) {
        return INVALID_DISTANCE_CM;
    }
    echoUs = (uint32_t)(hi_get_us() - start);
    if (echoUs == 0U || echoUs >= HCSR04_TIMEOUT_US) {
        return INVALID_DISTANCE_CM;
    }
    return echoUs * 0.034f / 2.0f;
}

static float Median(float a, float b, float c)
{
    float temp;

    if (a > b) {
        temp = a; a = b; b = temp;
    }
    if (b > c) {
        temp = b; b = c; c = temp;
    }
    if (a > b) {
        b = a;
    }
    return b;
}

static float ReadStableDistance(void)
{
    float a;
    float b;
    float c;

    (void)ReadDistance();
    osDelay(LOOP_DELAY_TICKS);
    a = ReadDistance();
    osDelay(LOOP_DELAY_TICKS);
    b = ReadDistance();
    osDelay(LOOP_DELAY_TICKS);
    c = ReadDistance();
    if (a < 0.0f || b < 0.0f || c < 0.0f) {
        return INVALID_DISTANCE_CM;
    }
    return Median(a, b, c);
}

static float ScanLeft(void)
{
    float distance;

    ServoLeft();
    osDelay(SERVO_SETTLE_TICKS);
    distance = ReadStableDistance();
    printf("[%s][SCAN] LEFT=%.1fcm PWM=%u\r\n", TASK_VERSION,
           distance, (unsigned int)SERVO_LEFT_US);
    return distance;
}

static float ScanRight(void)
{
    float distance;

    ServoCenter();
    ServoRight();
    osDelay(SERVO_SETTLE_TICKS);
    distance = ReadStableDistance();
    printf("[%s][SCAN] RIGHT=%.1fcm PWM=%u\r\n", TASK_VERSION,
           distance, (unsigned int)SERVO_RIGHT_US);
    return distance;
}

static int ChooseDirection(float left, float right, int *turnRight)
{
    int leftClear = left >= TURN_CLEAR_DISTANCE_CM;
    int rightClear = right >= TURN_CLEAR_DISTANCE_CM;

    if (!leftClear && !rightClear) {
        return 0;
    }
    if (leftClear && !rightClear) {
        *turnRight = 0;
    } else if (!leftClear && rightClear) {
        *turnRight = 1;
    } else if (left - right > TURN_TIE_CM) {
        *turnRight = 0;
    } else if (right - left > TURN_TIE_CM) {
        *turnRight = 1;
    } else {
        *turnRight = g_tieTurnRight;
        g_tieTurnRight = !g_tieTurnRight;
    }
    return 1;
}

static int FrontClear(void)
{
    float first = ReadDistance();

    osDelay(LOOP_DELAY_TICKS);
    return first >= RESUME_DISTANCE_CM &&
           ReadDistance() >= RESUME_DISTANCE_CM;
}

static int TurnUntilClear(int turnRight)
{
    uint32_t elapsed = 0;
    int left = turnRight ? TURN_SPEED : -TURN_SPEED;
    int right = turnRight ? -TURN_SPEED : TURN_SPEED;

    printf("[%s][ACTION] TURN_%s\r\n", TASK_VERSION,
           turnRight ? "RIGHT" : "LEFT");
    while (elapsed < MAX_TURN_TICKS) {
        RunMotorFor(left, right, TURN_CHUNK_TICKS);
        SendMotorCommand(0, 0);
        osDelay(TURN_SETTLE_TICKS);
        if (FrontClear()) {
            printf("[%s][ACTION] TURN_COMPLETE\r\n", TASK_VERSION);
            return 1;
        }
        elapsed += TURN_CHUNK_TICKS;
    }
    SendMotorCommand(0, 0);
    printf("[%s][ERROR] TURN_TIMEOUT\r\n", TASK_VERSION);
    return 0;
}

static int AvoidObstacle(float front)
{
    float left;
    float right;
    int turnRight;

    printf("[%s][OBSTACLE] FRONT=%.1fcm\r\n", TASK_VERSION, front);
    SendMotorCommand(0, 0);
    osDelay(STOP_WAIT_TICKS);
    left = ScanLeft();
    right = ScanRight();
    ServoCenter();
    osDelay(SERVO_SETTLE_TICKS);

    if (!ChooseDirection(left, right, &turnRight)) {
        printf("[%s][BLOCKED] LEFT=%.1f RIGHT=%.1f\r\n",
               TASK_VERSION, left, right);
        SendMotorCommand(0, 0);
        osDelay(BLOCKED_RETRY_TICKS);
        return 0;
    }
    printf("[%s][CHOICE] %s LEFT=%.1f RIGHT=%.1f\r\n",
           TASK_VERSION, turnRight ? "RIGHT" : "LEFT", left, right);
    return TurnUntilClear(turnRight);
}

static void AvoidanceTask(void *argument)
{
    uint32_t nearCount = 0;
    uint32_t safeCount = 0;
    uint32_t printCount = 0;
    int moving = 0;

    (void)argument;
    printf("[%s][BOOT] THREE_DIRECTION_AVOIDANCE\r\n", TASK_VERSION);
    SendMotorCommand(0, 0);
    ServoCenter();
    osDelay(START_DELAY_TICKS);

    while (1) {
        float front = ReadDistance();

        if (front < 0.0f) {
            SendMotorCommand(0, 0);
            moving = 0;
            nearCount = 0;
            safeCount = 0;
            printf("[%s][ERROR] FRONT_DIST=ERR STOPPED\r\n", TASK_VERSION);
            osDelay(LOOP_DELAY_TICKS);
            continue;
        }

        if (front < EMERGENCY_DISTANCE_CM) {
            nearCount = 2U;
        } else if (front < AVOID_DISTANCE_CM) {
            nearCount++;
        } else {
            nearCount = 0;
        }

        if (nearCount >= 2U) {
            SendMotorCommand(0, 0);
            moving = 0;
            safeCount = 0;
            (void)AvoidObstacle(front);
            nearCount = 0;
        } else if (front >= AVOID_DISTANCE_CM) {
            if (safeCount < 2U) {
                safeCount++;
            }
            if (safeCount >= 2U) {
                SendMotorCommand(FORWARD_LEFT, FORWARD_RIGHT);
                if (!moving) {
                    printf("[%s][ACTION] FORWARD L=%d R=%d\r\n",
                           TASK_VERSION, FORWARD_LEFT, FORWARD_RIGHT);
                }
                moving = 1;
            }
        } else {
            safeCount = 0;
        }

        if (++printCount >= 5U) {
            printf("[%s][SENSOR] FRONT=%.1fcm MOVING=%u\r\n",
                   TASK_VERSION, front, (unsigned int)moving);
            printCount = 0;
        }
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
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11,
              WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12,
              WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);
    UartInit(WIFI_IOT_UART_IDX_2, &uart, NULL);

    attr.name = "ultrasonic_avoidance";
    attr.stack_size = 6144U;
    attr.priority = osPriorityNormal;
    if (osThreadNew(AvoidanceTask, NULL, &attr) == NULL) {
        printf("[%s][BOOT] TASK_CREATE_FAILED\r\n", TASK_VERSION);
    }
}

APP_FEATURE_INIT(UltrasonicAvoidance);
