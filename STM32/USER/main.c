#include "stm32f10x.h"
#include "sys.h"
#include "encoder.h"
#include "motor.h"

int main(void)
{
    u8 front_light_command;

    Stm32_Clock_Init(9);               // 外部时钟8MHz，9倍频，8*9=72MHz
    MY_NVIC_PriorityGroupConfig(2);    // 中断优先级分组
    uart_init(115200);                 // 串口初始化为115200

    JTAG_Set(JTAG_SWD_DISABLE);        // 关闭JTAG接口
    JTAG_Set(SWD_ENABLE);              // 打开SWD接口，可以利用主板的SWD接口调试

    Encoder_Init_TIM2();               // 初始化TIM2左轮编码器（Read_Encoder(2)）
    Encoder_Init_TIM3();               // 初始化TIM3右轮编码器（Read_Encoder(3)）

    PWM_Init(7199, 9);                 // 初始化TIM4电机PWM，频率1kHz
    colorful_led_Init();               // 炫彩灯初始化

    SysTick_Config(72000000 / 1000);   // 滴答定时器，每1ms触发一次中断
    FrontLight_Off();                   // 上电默认关闭前灯
    R_led_CLC();                        // 上电默认关闭后灯

    printf("QST青软\r\n");

    /**主要程序**/
    while (1)
    {
        __disable_irq();
        front_light_command = g_front_light_command;
        g_front_light_command = FRONT_LIGHT_COMMAND_NONE;
        __enable_irq();

        if(front_light_command == FRONT_LIGHT_COMMAND_ON)
        {
            FrontLight_On();
            R_led_CLC();
        }
        else if(front_light_command == FRONT_LIGHT_COMMAND_LEFT)
        {
            FrontLight_Left_On();
            R_led_CLC();
        }
        else if(front_light_command == FRONT_LIGHT_COMMAND_RIGHT)
        {
            FrontLight_Right_On();
            R_led_CLC();
        }
        else if(front_light_command == FRONT_LIGHT_COMMAND_REAR)
        {
            FrontLight_Off();
            R_led_mode();
        }
        else if(front_light_command == FRONT_LIGHT_COMMAND_OFF)
        {
            FrontLight_Off();
            R_led_CLC();
        }

        delay_ms(10);
    }
}
