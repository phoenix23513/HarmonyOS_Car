#ifndef __MOTOR_H
#define __MOTOR_H

#include "sys.h"

#define MOTOR_PWM_MAX 7199

/* 左轮输出通道：PB14方向，PB7/TIM4_CH2 PWM（PWMA）。 */
#define AIN   PBout(14)
#define PWMA  TIM4->CCR2

/* 右轮输出通道：PB13方向，PB6/TIM4_CH1 PWM（PWMB）。 */
#define BIN   PBout(13)
#define PWMB  TIM4->CCR1

void Motor_Init(void);
void PWM_Init(u16 arr, u16 psc);
u32 myabs(long int a);

/*
 * 底层参数：moto1为右轮（PWMB/BIN），moto2为左轮（PWMA/AIN）。
 * 非负参数令对应方向脚为0，负参数令对应方向脚为1并反算CCR。
 * 当前闭环以Set_Pwm(Motor_B, Motor_A)实现左右反馈控制同侧电机；
 * 前进正目标组合和后退负目标组合已经在当前版本完成实车验证。
 */
void Set_Pwm(int moto1, int moto2);

/*
 * 高层接口参数顺序为左轮、右轮；函数名称只是原有接口名称，
 * 尚未实测确认每个组合与整车Forward/Backward/Turn/Spin名称一致。
 */
void Car_SetSpeed(int left_speed, int right_speed);
void Car_Stop(void);
void Car_Forward(int speed);
void Car_Backward(int speed);
void Car_TurnLeft(int speed);
void Car_TurnRight(int speed);
void Car_SpinLeft(int speed);
void Car_SpinRight(int speed);
void Car_Arc(int left_speed, int right_speed);

#endif
