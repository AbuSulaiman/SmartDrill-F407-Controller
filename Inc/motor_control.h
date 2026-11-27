#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H
#include "main.h"

void Motor_Control_Init(void);
void Motor_SetSpeed(uint8_t speed_percent);
void Motor_SetRunning(uint8_t running);
void Motor_ZeroCross_Callback(void);
void Motor_TIM_Delay_Callback(TIM_HandleTypeDef *htim);

#endif
