#include "motor_control.h"
#include "tim.h"

/* Initial settings for 50 Hz (10000 us per half-cycle).
 * Confirm PA0 edge timing and the active-low PA5 driver with an oscilloscope.
 * Speed percentage is a phase command, not a measured RPM setpoint. */
#define MIN_DELAY_US 300U
#define MAX_DELAY_US 9000U
#define GATE_PULSE_US 200U

typedef enum { GATE_IDLE, GATE_WAIT, GATE_PULSE } Gate_State;
static volatile Gate_State gate_state = GATE_IDLE;
static volatile uint8_t motor_running = 0;
static volatile uint8_t control_ready = 0;
static volatile uint8_t motor_speed_pct = 50;

static void Gate_Off(void)
{
    HAL_TIM_Base_Stop_IT(&htim3);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    HAL_GPIO_WritePin(TRIAC_TRIG_GPIO_Port, TRIAC_TRIG_Pin, GPIO_PIN_SET);
    gate_state = GATE_IDLE;
}

static void Arm_Timer(uint16_t delay_us)
{
    __HAL_TIM_DISABLE(&htim3);
    __HAL_TIM_SET_AUTORELOAD(&htim3, delay_us - 1U);
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
    if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK)
    {
        Gate_Off();
        Error_Handler();
    }
}

void Motor_Control_Init(void)
{
    motor_running = 0;
    motor_speed_pct = 50;
    Gate_Off();
    control_ready = 1;
}

void Motor_SetSpeed(uint8_t speed_percent)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    motor_speed_pct = (speed_percent > 100U) ? 100U : speed_percent;
    if (motor_speed_pct == 0U) Gate_Off();
    __set_PRIMASK(irq_state);
}

void Motor_SetRunning(uint8_t running)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    motor_running = running ? 1U : 0U;
    if (!motor_running) Gate_Off();
    __set_PRIMASK(irq_state);
}

void Motor_ZeroCross_Callback(void)
{
    /* EXTI is enabled before the timer is initialized at startup. */
    if (!control_ready) return;
    Gate_Off();
    if (!motor_running || motor_speed_pct == 0U) return;

    uint16_t delay_us = (uint16_t)(MAX_DELAY_US -
        ((uint32_t)motor_speed_pct * (MAX_DELAY_US - MIN_DELAY_US)) / 100U);
    gate_state = GATE_WAIT;
    Arm_Timer(delay_us);
}

void Motor_TIM_Delay_Callback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM3 || !control_ready) return;

    /* Keep a new zero-cross from interleaving with this state transition. */
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    HAL_TIM_Base_Stop_IT(&htim3);
    if (!motor_running || motor_speed_pct == 0U)
    {
        Gate_Off();
    }
    else if (gate_state == GATE_WAIT)
    {
        HAL_GPIO_WritePin(TRIAC_TRIG_GPIO_Port, TRIAC_TRIG_Pin, GPIO_PIN_RESET);
        gate_state = GATE_PULSE;
        Arm_Timer(GATE_PULSE_US);
    }
    else
    {
        Gate_Off();
    }
    __set_PRIMASK(irq_state);
}

