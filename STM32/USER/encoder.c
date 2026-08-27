#include "encoder.h"
#include "stm32f10x_gpio.h"

/**************************************************************************
函数功能：把TIM2初始化为左轮编码器接口模式（PA0、PA1）
入口参数：无
返回  值：无
**************************************************************************/
void Encoder_Init_TIM2(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);  // 使能TIM2时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 使能GPIOA端口时钟

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1; // 端口配置
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  // 浮空输入
    GPIO_Init(GPIOA, &GPIO_InitStructure);                 // 根据设定参数初始化GPIOA

    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Prescaler = 0x0;             // 预分频器
    TIM_TimeBaseStructure.TIM_Period = ENCODER_TIM_PERIOD; // 设定计数器自动重装值
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;// 选择时钟分频：不分频
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // TIM向上计数
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_EncoderInterfaceConfig(
        TIM2,
        TIM_EncoderMode_TI12,
        TIM_ICPolarity_Rising,
        TIM_ICPolarity_Rising
    );                                                      // 使用编码器模式3

    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_ICFilter = 10;
    TIM_ICInit(TIM2, &TIM_ICInitStructure);

    TIM_ClearFlag(TIM2, TIM_FLAG_Update);                   // 清除TIM的更新标志位
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    // Reset counter
    TIM_SetCounter(TIM2, 0);
    TIM_Cmd(TIM2, ENABLE);
}

/**************************************************************************
函数功能：把TIM3初始化为右轮编码器接口模式（PA6、PA7）
入口参数：无
返回  值：无
**************************************************************************/
void Encoder_Init_TIM3(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);  // 使能TIM3时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 使能GPIOA端口时钟

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7; // 端口配置
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  // 浮空输入
    GPIO_Init(GPIOA, &GPIO_InitStructure);                 // 根据设定参数初始化GPIOA

    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Prescaler = 0x0;             // 预分频器
    TIM_TimeBaseStructure.TIM_Period = ENCODER_TIM_PERIOD; // 设定计数器自动重装值
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;// 选择时钟分频：不分频
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // TIM向上计数
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_EncoderInterfaceConfig(
        TIM3,
        TIM_EncoderMode_TI12,
        TIM_ICPolarity_Rising,
        TIM_ICPolarity_Rising
    );                                                      // 使用编码器模式3

    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_ICFilter = 10;
    TIM_ICInit(TIM3, &TIM_ICInitStructure);

    TIM_ClearFlag(TIM3, TIM_FLAG_Update);                   // 清除TIM的更新标志位
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    // Reset counter
    TIM_SetCounter(TIM3, 0);
    TIM_Cmd(TIM3, ENABLE);
}

/**************************************************************************
函数功能：单位时间读取编码器计数
入口参数：TIMX=2读取左轮，TIMX=3读取右轮，其他值返回0
返回  值：本采样周期的有符号编码器计数；读取后对应计数器清零
**************************************************************************/
static long Encoder_TIM_A_now;
static long Encoder_TIM_B_now;

int Read_Encoder(u8 TIMX)
{
    int Encoder_TIM;

    switch (TIMX)
    {
        case 2:
            TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
            Encoder_TIM_A_now = (short)TIM2->CNT; // 左轮本周期有符号计数，正负号已实车确认
            Encoder_TIM = Encoder_TIM_A_now;
            TIM_SetCounter(TIM2, 0);
            break;

        case 3:
            TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
            Encoder_TIM_B_now = (short)TIM3->CNT; // 右轮本周期有符号计数，正负号已实车确认
            Encoder_TIM = Encoder_TIM_B_now;
            TIM_SetCounter(TIM3, 0);
            break;

        default:
            Encoder_TIM = 0;
    }

    return Encoder_TIM;
}
