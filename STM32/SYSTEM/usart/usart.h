#ifndef __USART_H
#define __USART_H

#define FRONT_LIGHT_COMMAND_NONE  0
#define FRONT_LIGHT_COMMAND_OFF   1
#define FRONT_LIGHT_COMMAND_ON    2
#include "stdio.h"	
#include "sys.h" 

#define USART_REC_LEN  			200  	//定义最大接收字节数 200
#define EN_USART1_RX 			1		//使能（1）/禁止（0）串口1接收

extern u8  USART_RX_BUF[USART_REC_LEN]; //接收缓冲,最大USART_REC_LEN个字节.末字节为换行符 
extern volatile u8 USART_RX_STA;        		//接收状态标记	volatile 表示这个变量可能被串口中断异步修改
extern volatile u8 g_front_light_command;
//如果想串口中断接收，请不要注释以下宏定义
void uart_init(u32 bound);

#endif


