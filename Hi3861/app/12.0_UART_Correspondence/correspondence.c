#include <stdint.h>
#include <stdio.h>

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"
#include "wifiiot_watchdog.h"

#define MOTOR_FRAME_HEAD         0xFC
#define MOTOR_FRAME_TAIL         0xFD
#define MOTOR_SPEED_MAX_X100     150
#define DRIVE_TIME_TICKS         200U
#define LEFT_TURN_TIME_TICKS     70U
#define RIGHT_TURN_TIME_TICKS    120U
#define COMMAND_REFRESH_TICKS    20U
#define TRANSITION_STEPS         4
#define TRANSITION_STEP_TICKS    10U
#define SEQUENCE_PAUSE_TICKS     200U
#define TASK_VERSION             "TASK24_V3"

static uint8_t g_uartSendBuffer[6];
static int g_currentLeftSpeedX100 = 0;
static int g_currentRightSpeedX100 = 0;

/* 参数单位为0.01转/秒；正数前进，负数后退。 */
static void Stm32MotorControl(int leftSpeedX100, int rightSpeedX100)
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
    if (leftSpeedX100 > MOTOR_SPEED_MAX_X100) {
        leftSpeedX100 = MOTOR_SPEED_MAX_X100;
    }
    if (rightSpeedX100 > MOTOR_SPEED_MAX_X100) {
        rightSpeedX100 = MOTOR_SPEED_MAX_X100;
    }

    g_uartSendBuffer[0] = MOTOR_FRAME_HEAD;
    g_uartSendBuffer[1] = leftDirection;
    g_uartSendBuffer[2] = (uint8_t)leftSpeedX100;
    g_uartSendBuffer[3] = rightDirection;
    g_uartSendBuffer[4] = (uint8_t)rightSpeedX100;
    g_uartSendBuffer[5] = MOTOR_FRAME_TAIL;
    UartWrite(WIFI_IOT_UART_IDX_2, g_uartSendBuffer,
              sizeof(g_uartSendBuffer));

    printf("[%s][TX] FC %02X %02X %02X %02X FD\r\n",
           TASK_VERSION, g_uartSendBuffer[1], g_uartSendBuffer[2],
           g_uartSendBuffer[3], g_uartSendBuffer[4]);
}

static void HoldMotorSpeed(int leftSpeedX100, int rightSpeedX100,
                           uint32_t durationTicks)
{
    uint32_t elapsedTicks = 0;

    while (elapsedTicks < durationTicks) {
        Stm32MotorControl(leftSpeedX100, rightSpeedX100);
        osDelay(COMMAND_REFRESH_TICKS);
        elapsedTicks += COMMAND_REFRESH_TICKS;
    }
}

static void TransitionMotorSpeed(int targetLeftSpeedX100,
                                 int targetRightSpeedX100)
{
    int step;
    int startLeftSpeedX100 = g_currentLeftSpeedX100;
    int startRightSpeedX100 = g_currentRightSpeedX100;

    printf("[%s][TRANSITION] L=%d->%d R=%d->%d\r\n", TASK_VERSION,
           startLeftSpeedX100, targetLeftSpeedX100,
           startRightSpeedX100, targetRightSpeedX100);
    for (step = 1; step <= TRANSITION_STEPS; step++) {
        g_currentLeftSpeedX100 = startLeftSpeedX100 +
            (targetLeftSpeedX100 - startLeftSpeedX100) * step /
            TRANSITION_STEPS;
        g_currentRightSpeedX100 = startRightSpeedX100 +
            (targetRightSpeedX100 - startRightSpeedX100) * step /
            TRANSITION_STEPS;
        Stm32MotorControl(g_currentLeftSpeedX100,
                          g_currentRightSpeedX100);
        osDelay(TRANSITION_STEP_TICKS);
    }
}

static void CarStop(void)
{
    printf("[%s][ACTION] STOP\r\n", TASK_VERSION);
    TransitionMotorSpeed(0, 0);
}

static void RunAction(const char *actionName, int leftSpeedX100,
                      int rightSpeedX100,
                      uint32_t durationTicks)
{
    printf("[%s][ACTION] %s\r\n", TASK_VERSION, actionName);
    TransitionMotorSpeed(leftSpeedX100, rightSpeedX100);
    HoldMotorSpeed(leftSpeedX100, rightSpeedX100, durationTicks);
}

static void CorrespondenceTask(void *argument)
{
    (void)argument;
    printf("[%s][BOOT] UART2=115200,8N1\r\n", TASK_VERSION);
    Stm32MotorControl(0, 0);

    while (1) {
        RunAction("FORWARD", 120, 116, DRIVE_TIME_TICKS);
        RunAction("BACKWARD", -120, -120, DRIVE_TIME_TICKS);
        RunAction("LEFT", 20, 120, LEFT_TURN_TIME_TICKS);
        RunAction("RIGHT", 120, 20, RIGHT_TURN_TIME_TICKS);
        printf("[%s][SEQUENCE] COMPLETE\r\n", TASK_VERSION);
        CarStop();
        osDelay(SEQUENCE_PAUSE_TICKS);
    }
}

static void Correspondence(void)
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
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11,
              WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12,
              WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);
    UartInit(WIFI_IOT_UART_IDX_2, &uartAttribute, NULL);

    threadAttribute.name = "correspondence";
    threadAttribute.stack_size = 4096;
    threadAttribute.priority = osPriorityNormal;
    if (osThreadNew(CorrespondenceTask, NULL, &threadAttribute) == NULL) {
        printf("[%s][BOOT] TASK_CREATE=FAILED\r\n", TASK_VERSION);
    }
}

APP_FEATURE_INIT(Correspondence);
