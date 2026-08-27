#ifndef __ENCODER_H
#define __ENCODER_H

#include "sys.h"

#define ENCODER_TIM_PERIOD 65535

void Encoder_Init_TIM2(void); // TIM2/PA0、PA1：左轮编码器
void Encoder_Init_TIM3(void); // TIM3/PA6、PA7：右轮编码器
int Read_Encoder(u8 TIMX);       // 参数2读左轮，参数3读右轮；符号已实车确认

#endif
