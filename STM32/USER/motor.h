#ifndef __MOTOR_H
#define __MOTOR_H

#include "sys.h"

#define MOTOR_PWM_MAX 7199

/* Left motor: PB14 direction, PB7/TIM4_CH2 PWM. */
#define AIN   PBout(14)
#define PWMA  TIM4->CCR2

/* Right motor: PB13 direction, PB6/TIM4_CH1 PWM. */
#define BIN   PBout(13)
#define PWMB  TIM4->CCR1

void Motor_Init(void);
void PWM_Init(u16 arr, u16 psc);
u32 myabs(long int a);

/* Low-level interface: moto1 is right, moto2 is left. */
void Set_Pwm(int moto1, int moto2);

/* High-level interfaces use the natural left/right order. */
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
