#include "sys.h"
#include "usart.h"	  

//////////////////////////////////////////////////////////////////
//加入以下代码,支持printf函数,而不需要选择use MicroLIB	  
#if 1
#pragma import(__use_no_semihosting)             
//标准库需要的支持函数                 
struct __FILE 
{ 
	int handle; 

}; 

FILE __stdout;       
//定义_sys_exit()以避免使用半主机模式    
_sys_exit(int x) 
{ 
	x = x; 
} 
//重定义fputc函数 
int fputc(int ch, FILE *f)
{      
	while((USART1->SR&0X40)==0);//循环发送,直到发送完毕   
    USART1->DR = (u8) ch;      
	return ch;
}
#endif 


#if EN_USART1_RX   //如果使能了接收

u8 USART_RX_BUF[USART_REC_LEN];     //接收缓冲,最大USART_REC_LEN个字节.
volatile u8 USART_RX_STA = 0;      //接收状态标记	  
volatile u8 g_front_light_command = FRONT_LIGHT_COMMAND_NONE;
volatile u8 g_motor_command[4] = {0, 0, 0, 0};
volatile u8 g_motor_frame_ready = 0;
u8 count=0;

/* 帧格式：FA 4C 状态 校验 FB，校验字节为 4C ^ 状态。 */
static u8 FrontLight_ParseByte(u8 data)
{
    static u8 frame_state = 0;
    static u8 light_state = 0;

    switch(frame_state)
    {
        case 0:
            if(data == 0xFA)
            {
                frame_state = 1;
                return 1;
            }
            return 0;

        case 1:
            frame_state = (data == 0x4C) ? 2 : ((data == 0xFA) ? 1 : 0);
            return 1;

        case 2:
            if(data == 0x00 || data == 0x01)
            {
                light_state = data;
                frame_state = 3;
            }
            else
            {
                frame_state = (data == 0xFA) ? 1 : 0;
            }
            return 1;

        case 3:
            frame_state = (data == (u8)(0x4C ^ light_state)) ? 4 : 0;
            return 1;

        case 4:
            if(data == 0xFB)
            {
                g_front_light_command = light_state ?
                    FRONT_LIGHT_COMMAND_ON : FRONT_LIGHT_COMMAND_OFF;
            }
            frame_state = 0;
            return 1;

        default:
            frame_state = 0;
            return 0;
    }
}

/* 帧格式：FC 左方向 左速度 右方向 右速度 FD。 */
static u8 MotorControl_ParseByte(u8 data)
{
    static u8 frame[6];
    static u8 frame_index = 0;

    if(data == 0xFC)
    {
        frame[0] = data;
        frame_index = 1;
        return 1;
    }
    if(frame_index == 0)
        return 0;

    frame[frame_index++] = data;
    if(frame_index < 6)
        return 1;

    frame_index = 0;
    if(frame[5] != 0xFD || frame[1] > 1 || frame[2] > 150 ||
       frame[3] > 1 || frame[4] > 150)
        return 1;

    g_motor_command[0] = frame[1];
    g_motor_command[1] = frame[2];
    g_motor_command[2] = frame[3];
    g_motor_command[3] = frame[4];
    g_motor_frame_ready = 1;
    return 1;
}

void uart_init(u32 bound){
  //GPIO端口设置
  GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1|RCC_APB2Periph_GPIOA, ENABLE);	//使能USART1，GPIOA时钟
  
	//USART1_TX   GPIOA.9
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; //PA.9
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	//复用推挽输出
  GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化GPIOA.9
   
  //USART1_RX	  GPIOA.10初始化
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;//PA10
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//浮空输入
  GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化GPIOA.10  

  //Usart1 NVIC 配置
  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=3 ;//抢占优先级3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;		//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
  
   //USART 初始化设置

	USART_InitStructure.USART_BaudRate = bound;//串口波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式

  USART_Init(USART1, &USART_InitStructure); //初始化串口1
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);//开启串口接受中断
  USART_Cmd(USART1, ENABLE);                    //使能串口1 

}

void USART1_IRQHandler(void)
{
    u8 Res;

    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        Res = (u8)USART_ReceiveData(USART1);

        if(MotorControl_ParseByte(Res))
        {
            USART_ClearFlag(USART1, USART_FLAG_RXNE);
            return;
        }

        /* 二进制灯光控制帧不进入原有文本命令缓冲区。 */
        if(FrontLight_ParseByte(Res))
        {
            USART_ClearFlag(USART1, USART_FLAG_RXNE);
            return;
        }

        /* 回车或换行用于重新开始接收一条命令 */
        if(Res == '\r' || Res == '\n')
        {
            count = 0;
        }
        else
        {
            USART_RX_BUF[count] = Res;
            count++;

            /* 当前命令长度固定为 5 个字符 */
            if(count == 5)
            {
                if(USART_RX_BUF[0] == 'H' &&
                   USART_RX_BUF[1] == 'E' &&
                   USART_RX_BUF[2] == 'L' &&
                   USART_RX_BUF[3] == 'L' &&
                   USART_RX_BUF[4] == 'O')
                {
                    USART_RX_STA = 1;
                }
                else if(USART_RX_BUF[0] == 'S' &&
                        USART_RX_BUF[1] == 'T' &&
                        USART_RX_BUF[2] == 'O' &&
                        USART_RX_BUF[3] == 'P' &&
                        USART_RX_BUF[4] == '!')
                {
                    USART_RX_STA = 2;
                }

                count = 0;
            }
        }
    }

    USART_ClearFlag(USART1, USART_FLAG_RXNE);
}

#endif	

