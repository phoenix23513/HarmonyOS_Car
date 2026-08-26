# 双芯片智能小车学习项目

这是一个基于 **STM32F103C8** 和 **Hi3861** 的智能小车学习项目，用于记录嵌入式开发、OpenHarmony、Linux 和 Git 的实践过程。

## 项目结构

- **STM32**：负责车轮电机、PWM、车灯和串口等实时硬件控制。
- **Hi3861**：负责运行 OpenHarmony，完成多线程、互斥锁、SG90 舵机和网络相关实验。

## 已完成内容

- OpenHarmony 多线程周期输出实验。
- 使用互斥锁保护多个线程共享的舵机控制资源。
- 使用 PWM 控制 SG90 舵机转向 90° 和 180°。
- 使用 STM32 TIM4 PWM 控制左右车轮速度和方向。
- 实现前进、后退、弧线转弯和原地旋转等运动形式。
- 通过串口指令控制小车彩灯效果。
- 完成 Keil、ST-Link、HiBurn、UartAssist 和 Remote-SSH 的实际使用。

## 仓库目录

```text
├── STM32/                       STM32 源码和硬件驱动
├── Hi3861/                      Hi3861 功能支持源码
├── learning_log.md              每日学习与开发记录
├── linux_terminal_git_guide.md  Linux、SSH 和 Git 学习笔记
└── sync_log.txt                 项目同步记录
```

本仓库仅用于学习成果和代码展示，不作为实际开发与编译环境。完整工程、开发工具、安装包和编译产物保留在本地开发目录中。
