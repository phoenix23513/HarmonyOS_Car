#include "control_system.h"
#include "encoder_control.h"

int L_speed = 0;
int R_speed = 0;
int OverflowTime = 100;

volatile uint32_t millis = 0;  // 记录毫秒数
volatile uint32_t seconds = 0; // 记录秒数

/**************************************************************************
函数功能：系统控制函数
入口参数：
返回  值：
**************************************************************************/
void System_Control(void)
{
    L_speed = Read_Encoder(2); // 读取OverflowTime ms时间的脉冲数
    R_speed = Read_Encoder(3);

    printf("left  speed : %d\r\n", L_speed);
    printf("right speed : %d\r\n", R_speed);
}

/**
  * @brief  系统滴答定时器中断服务函数
  * @param  None
  * @retval : None
  */

