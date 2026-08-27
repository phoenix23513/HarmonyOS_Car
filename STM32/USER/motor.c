#include "motor.h"

static int LimitSpeed(int speed)
{
    if (speed > MOTOR_PWM_MAX)
        return MOTOR_PWM_MAX;

    if (speed < -MOTOR_PWM_MAX)
        return -MOTOR_PWM_MAX;

    return speed;
}

static int PositiveSpeed(int speed)
{
    if (speed < 0)
        speed = -speed;

    return LimitSpeed(speed);
}

void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    AIN = 0;
    BIN = 0;
}

void PWM_Init(u16 arr, u16 psc)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    Motor_Init();

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    TIM_TimeBaseStructure.TIM_Period = arr;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    TIM_OC1Init(TIM4, &TIM_OCInitStructure);
    TIM_OC2Init(TIM4, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM4, ENABLE);
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM4, ENABLE);
    TIM_Cmd(TIM4, ENABLE);
}

u32 myabs(long int a)
{
    if (a < 0)
        return (u32)(-a);

    return (u32)a;
}

/*
 * 底层电机接口：
 * moto1控制右轮，写入PWMB/TIM4_CH1（PB6），方向脚为BIN/PB13；
 * moto2控制左轮，写入PWMA/TIM4_CH2（PB7），方向脚为AIN/PB14。
 * 参数非负时方向脚为0，CCR写入参数绝对值；参数为负时方向脚为1，
 * CCR写入MOTOR_PWM_MAX减去参数绝对值。
 * 当前System_Control调用Set_Pwm(Motor_B, Motor_A)，使右、左轮闭环输出
 * 分别进入第1、第2参数并驱动同侧电机；该对应关系已经实车验证。
 */
void Set_Pwm(int moto1, int moto2)
{
    if (moto2 >= 0)
    {
        AIN = 0;
        PWMA = myabs(moto2);
    }
    else
    {
        AIN = 1;
        PWMA = MOTOR_PWM_MAX - myabs(moto2);
    }

    if (moto1 >= 0)
    {
        BIN = 0;
        PWMB = myabs(moto1);
    }
    else
    {
        BIN = 1;
        PWMB = MOTOR_PWM_MAX - myabs(moto1);
    }
}

/*
 * 高层参数顺序为左轮、右轮，内部交换后传给Set_Pwm。
 * 以下Forward/Backward等函数名沿用原接口；在方向组合完成实车复核前，
 * 不能仅根据函数名认定整车的实际运动方向。
 */
void Car_SetSpeed(int left_speed, int right_speed)
{
    left_speed = LimitSpeed(left_speed);
    right_speed = LimitSpeed(right_speed);

    Set_Pwm(right_speed, left_speed);
}

void Car_Stop(void)
{
    Car_SetSpeed(0, 0);
}

void Car_Forward(int speed)
{
    speed = PositiveSpeed(speed);
    Car_SetSpeed(speed, speed);
}

void Car_Backward(int speed)
{
    speed = PositiveSpeed(speed);
    Car_SetSpeed(-speed, -speed);
}

void Car_TurnLeft(int speed)
{
    speed = PositiveSpeed(speed);
    Car_SetSpeed(0, speed);
}

void Car_TurnRight(int speed)
{
    speed = PositiveSpeed(speed);
    Car_SetSpeed(speed, 0);
}

void Car_SpinLeft(int speed)
{
    speed = PositiveSpeed(speed);
    Car_SetSpeed(-speed, speed);
}

void Car_SpinRight(int speed)
{
    speed = PositiveSpeed(speed);
    Car_SetSpeed(speed, -speed);
}

void Car_Arc(int left_speed, int right_speed)
{
    Car_SetSpeed(left_speed, right_speed);
}
