#include "stm32f10x.h"
#include "sys.h"
#include "motor.h"

#define DRIVE_SPEED  3000
#define TURN_SPEED   3000
#define ARC_SLOW     1800
#define ARC_FAST     3500

static void StopAndWait(void)
{
    Car_Stop();
    delay_ms(1000);
}

static void RunMotionDemo(void)
{
    printf("FORWARD\r\n");
    Car_Forward(DRIVE_SPEED);
    delay_ms(2000);
    StopAndWait();

    printf("BACKWARD\r\n");
    Car_Backward(DRIVE_SPEED);
    delay_ms(2000);
    StopAndWait();

    printf("TURN LEFT\r\n");
    Car_TurnLeft(TURN_SPEED);
    delay_ms(1500);
    StopAndWait();

    printf("TURN RIGHT\r\n");
    Car_TurnRight(TURN_SPEED);
    delay_ms(1500);
    StopAndWait();

    printf("SPIN LEFT\r\n");
    Car_SpinLeft(TURN_SPEED);
    delay_ms(1200);
    StopAndWait();

    printf("SPIN RIGHT\r\n");
    Car_SpinRight(TURN_SPEED);
    delay_ms(1200);
    StopAndWait();

    printf("ARC LEFT\r\n");
    Car_Arc(ARC_SLOW, ARC_FAST);
    delay_ms(2000);
    StopAndWait();

    printf("ARC RIGHT\r\n");
    Car_Arc(ARC_FAST, ARC_SLOW);
    delay_ms(2000);
    StopAndWait();

    printf("MOTION DEMO FINISHED\r\n");
    Car_Stop();
}

int main(void)
{
    RCC->CSR |= 1 << 24;
    Stm32_Clock_Init(9);
    MY_NVIC_PriorityGroupConfig(2);
    uart_init(115200);
    JTAG_Set(JTAG_SWD_DISABLE);
    JTAG_Set(SWD_ENABLE);
    PWM_Init(7199, 9);
    colorful_led_Init();

    printf("MOTION DEMO START\r\n");
    delay_ms(2000);
    RunMotionDemo();

    while (1)
    {
        delay_ms(100);
    }
}
