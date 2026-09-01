#include "encoder_control.h"
#include "motor.h"
#include "usart.h"
//typedef enum {false = 0, true = 1} bool;

#define MOTOR_COMMAND_TIMEOUT_CYCLES 15
#define TURN_SPEED_DIFFERENCE_X100 30

/*
 * 闭环A：TIM2左轮反馈 -> Motor_A -> Set_Pwm第2参数 -> PWMA/AIN左轮。
 * 闭环B：TIM3右轮反馈 -> Motor_B -> Set_Pwm第1参数 -> PWMB/BIN右轮。
 * 当前左右反馈分别控制同侧电机，反馈串轮已经消除。
 */
int L_coder, R_coder;

int Motor_A, Motor_B;          // Motor_A为左轮闭环输出，Motor_B为右轮闭环输出
int OverflowTime = 100;
volatile uint32_t millis = 0;

static void SetMotionLightCommand(u8 command)
{
    static u8 last_command = FRONT_LIGHT_COMMAND_NONE;

    if (command != last_command)
    {
        g_front_light_command = command;
        last_command = command;
    }
}

/**************************************************************************
函数功能：当前速度闭环累加控制器（函数名保留Incremental_PI）
入口参数：对应车轮的编码器测量值、目标计数
返回  值：对应车轮的有符号控制输出

标准增量式离散PID公式参考：
pwm += Kp[e(k)-e(k-1)] + Ki*e(k)
     + Kd[e(k)-2e(k-1)+e(k-2)]

e(k)代表本次偏差
e(k-1)代表上一次的偏差，以此类推
pwm代表增量输出

当前代码实际执行：
pwm += Kp*e(k) + Kd[e(k)-e(k-1)]
Velocity_KI只用于计算积分限幅，Integral也会累计，但二者未加入输出公式；
因此当前实现不是标准增量PI，调参时不能把KI当作已生效。
**************************************************************************/
/* A闭环使用TIM2左轮反馈；Motor_A经Set_Pwm第2参数驱动左轮。 */
int Incremental_PI_A(int Encoders_A, int Target_A)
{
    float Velocity_KP = 7.0;
    float Velocity_KI = 0.016;
    float Velocity_KD = 0.003;

    static int Pwm_A = 0;
    static int Integral_A = 0;
    static float Error_prev_A = 0;

    float MaxIntegral = 0.0;
    float MinIntegral = 0.0;
    float Error_A = (float)(Target_A - Encoders_A); // 计算偏差

    if (Target_A == 0)
    {
        Pwm_A = 0;
        Integral_A = 0;
        Error_prev_A = 0;
        return 0;
    }

    Integral_A += Error_A; // 积分项更新

    // 积分限幅
    MaxIntegral = (float)(7199 / Velocity_KI);
    MinIntegral = -(float)(7199 / Velocity_KI);

    if (Integral_A > MaxIntegral)
        Integral_A = MaxIntegral;
    else if (Integral_A < MinIntegral)
        Integral_A = MinIntegral;

    Pwm_A += Velocity_KP * Error_A
           + Velocity_KD * (Error_A - Error_prev_A);

    if (Pwm_A > 7199)
        Pwm_A = 7199;
    else if (Pwm_A < -7199)
        Pwm_A = -7199;

    Error_prev_A = Error_A; // 保存上一次偏差

    return Pwm_A; // 增量输出
}

/* B闭环使用TIM3右轮反馈；Motor_B经Set_Pwm第1参数驱动右轮。 */
int Incremental_PI_B(int Encoders_B, int Target_B)
{
    /*
     * 当前三个参数与Incremental_PI_A相同。
     * 如需继续补偿两侧电机批次、安装阻力或死区差异，应根据左右轮反馈
     * 分别整定；当前KI未进入输出公式，调整它不会改变PWM。
     */
    float Velocity_KP = 7.0;
    float Velocity_KI = 0.016;
    float Velocity_KD = 0.003;

    static int Pwm_B = 0;
    static int Integral_B = 0;
    static float Error_prev_B = 0;

    float MaxIntegral = 0.0;
    float MinIntegral = 0.0;
    float Error_B = (float)(Target_B - Encoders_B); // 计算偏差

    if (Target_B == 0)
    {
        Pwm_B = 0;
        Integral_B = 0;
        Error_prev_B = 0;
        return 0;
    }

    Integral_B += Error_B; // 积分项更新

    // 积分限幅
    MaxIntegral = (float)(7199 / Velocity_KI);
    MinIntegral = -(float)(7199 / Velocity_KI);

    if (Integral_B > MaxIntegral)
        Integral_B = MaxIntegral;
    else if (Integral_B < MinIntegral)
        Integral_B = MinIntegral;

    Pwm_B += Velocity_KP * Error_B
           + Velocity_KD * (Error_B - Error_prev_B);

    if (Pwm_B > 7199)
        Pwm_B = 7199;
    else if (Pwm_B < -7199)
        Pwm_B = -7199;

    Error_prev_B = Error_B; // 保存上一次偏差

    return Pwm_B; // 增量输出
}

/**************************************************************************
函数功能：转每秒转脉冲数函数
入口参数：float
返回  值：int

电机PPR：700，倍频4。
设定电机转速为1转/s，已知电机1转产生(700*4)脉冲，
则每100ms产生的脉冲数为：
(700*4)/(1000/100)，单位：脉冲数/100ms
**************************************************************************/
int Rs_To_CPR(float rads)
{
    // rads取值范围：-1.5～1.5，即最大设定转速为1.5转/s
    int CRP = 0;

    CRP = rads * ((700 * 4) / (1000 / OverflowTime));

    return CRP;
}

/**************************************************************************
函数功能：根据UART协议目标执行左右轮速度闭环
入口参数：无
返回  值：无
说明：每100ms调用一次；1.5秒未收到有效控制帧时自动停车
**************************************************************************/
void System_Control(void)
{
    static float left_target_rps = 0.0f;
    static float right_target_rps = 0.0f;
    static int command_timeout_cycles = MOTOR_COMMAND_TIMEOUT_CYCLES;
    u8 left_direction;
    u8 left_speed;
    u8 right_direction;
    u8 right_speed;
    int TageA;
    int TageB;

    if (g_motor_frame_ready)
    {
        left_direction = g_motor_command[0];
        left_speed = g_motor_command[1];
        right_direction = g_motor_command[2];
        right_speed = g_motor_command[3];
        g_motor_frame_ready = 0;

        left_target_rps = left_speed / 100.0f;
        right_target_rps = right_speed / 100.0f;
        if (left_direction == 1)
            left_target_rps = -left_target_rps;
        if (right_direction == 1)
            right_target_rps = -right_target_rps;

        if (left_speed == 0 && right_speed == 0)
            SetMotionLightCommand(FRONT_LIGHT_COMMAND_OFF);
        else if (left_direction == 1 && right_direction == 1)
            SetMotionLightCommand(FRONT_LIGHT_COMMAND_REAR);
        else if (left_direction == 0 && right_direction == 0)
        {
            if ((int)right_speed - (int)left_speed >= TURN_SPEED_DIFFERENCE_X100)
                SetMotionLightCommand(FRONT_LIGHT_COMMAND_LEFT);
            else if ((int)left_speed - (int)right_speed >= TURN_SPEED_DIFFERENCE_X100)
                SetMotionLightCommand(FRONT_LIGHT_COMMAND_RIGHT);
            else
                SetMotionLightCommand(FRONT_LIGHT_COMMAND_ON);
        }
        else
            SetMotionLightCommand(FRONT_LIGHT_COMMAND_OFF);

        command_timeout_cycles = 0;
    }
    else if (command_timeout_cycles < MOTOR_COMMAND_TIMEOUT_CYCLES)
    {
        command_timeout_cycles++;
    }

    if (command_timeout_cycles >= MOTOR_COMMAND_TIMEOUT_CYCLES)
    {
        left_target_rps = 0.0f;
        right_target_rps = 0.0f;
        SetMotionLightCommand(FRONT_LIGHT_COMMAND_OFF);
    }

    // 每OverflowTime ms读取并清零：TIM2为左轮，TIM3为右轮
    L_coder = Read_Encoder(2);
    R_coder = Read_Encoder(3);
    TageA = Rs_To_CPR(left_target_rps);
    TageB = Rs_To_CPR(right_target_rps);

    // 左轮反馈计算Motor_A，右轮反馈计算Motor_B；输出写入同侧电机
    Motor_A = Incremental_PI_A(L_coder, TageA);
    Motor_B = Incremental_PI_B(R_coder, TageB);
    Set_Pwm(Motor_B, Motor_A); // 第1参数驱动右轮，第2参数驱动左轮
}


