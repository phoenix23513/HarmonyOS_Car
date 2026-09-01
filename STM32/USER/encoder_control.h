#ifndef __CONTROL_SYSTEM_H
#define __CONTROL_SYSTEM_H

#include "sys.h"
#include "encoder.h"

extern int L_speed;
extern int R_speed;
extern int OverflowTime;             // 闭环采样周期，单位ms
extern volatile uint32_t millis;   // SysTick累计毫秒数

void System_Control(void);              // 根据UART协议目标执行左右轮同侧闭环控制

#endif
