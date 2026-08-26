#include "motor.h"

/**************************************************************************
函数功能：初始化电机方向
入口参数：无
返回  值：无
**************************************************************************/
void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);  // 使能PB端口时钟

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_13; // 端口配置
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;         // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;        // 50M
    GPIO_Init(GPIOB, &GPIO_InitStructure);                    // 初始化GPIOB

    AIN = 0;
    BIN = 0;
}

/**************************************************************************
函数功能：初始化定时器PWM
入口参数：无
返回  值：无
**************************************************************************/
void PWM_Init(u16 arr, u16 psc)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    Motor_Init();

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // 使能GPIO外设时钟

    // 设置引脚为复用输出功能，输出PWM脉冲波形
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;       // 复用推挽输出
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
    u32 temp;

    if (a < 0)
        temp = -a;
    else
        temp = a;

    return temp;
}

void Set_Pwm(int moto1, int moto2)
{
    // XIN、PWMX在motor.h中有定义
    if (moto2 >= 0)
    {
        AIN = 0;
        PWMA = myabs(moto2);
    }
    else
    {
        AIN = 1;
        PWMA = 7199 - myabs(moto2);
    }

    if (moto1 >= 0)
    {
        BIN = 0;
        PWMB = myabs(moto1);
    }
    else
    {
        BIN = 1;
        PWMB = 7199 - myabs(moto1);
    }
}