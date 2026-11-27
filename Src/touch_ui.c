/* SmartDrill F407 V5.2 - Instant Touch, EXTI RPM/HZ, Zero-Wipe UI & Fixed XC Check */
#include "main.h"
#include "ili9341.h"
#include "touch_calibration.h"
#include <stdio.h>

#define C_BG        0x0000
#define C_PANEL     0x18E3
#define C_BORDER    0x4228
#define C_WHITE     0xFFFF
#define C_CYAN      0x07FF
#define C_GREEN     0x07E0
#define C_RED       0xF800
#define C_YELLOW    0xFFE0
#define C_ORANGE    0xFD20

extern uint16_t touch_read(uint8_t command);
extern volatile uint32_t touch_errors, boot_stage;

static const int px[5] = {20, 219, 20, 219, 120};
static const int py[5] = {20, 20, 299, 299, 160};
volatile uint16_t cal_raw_x[5], cal_raw_y[5];
volatile uint32_t cal_valid;
static float ax, bx, cx, ay, by, cy;

static uint32_t pot_value;
static uint32_t pot_filtered;
static uint32_t pot_fault;
static volatile uint32_t speed_percent;
static volatile uint32_t speed_permille;
static volatile uint32_t gate_allow;
static volatile uint32_t emergency_latched;
static uint8_t ui_running;

static volatile uint32_t hall_irq_edges;
static volatile uint32_t zc_irq_edges;
static volatile uint32_t last_hall_activity_tick;
static volatile uint32_t zc_capture_period_us;
static volatile uint32_t zc_capture_last;

static uint32_t hall_rate_hz, motor_rpm, zc_rate_hz;
static uint32_t sensor_disconnected = 0u;
static uint32_t run_start_tick;
static uint32_t last_hw_start;

static uint32_t prev_speed   = 999u;
static uint32_t prev_rpm     = 999999u;
static uint32_t prev_hz      = 999999u;
static uint32_t prev_zc_hz   = 999999u;
static uint8_t  prev_running = 255u;
static uint32_t prev_err     = 999u;
static uint32_t prev_hw_s    = 99u;
static uint32_t prev_hw_t    = 99u;

static void inputs_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    g.Pin  = GPIO_PIN_3;
    g.Mode = GPIO_MODE_IT_FALLING;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_NVIC_SetPriority(EXTI3_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);

#ifdef TOACH_ZC_STAGE3
    g.Pin = GPIO_PIN_0;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &g);

    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM2->PSC = 15;
    TIM2->ARR = 0xFFFFFFFFu;
    TIM2->CCMR1 = TIM_CCMR1_CC1S_0 | (3u << 4);
    TIM2->CCER = TIM_CCER_CC1E | TIM_CCER_CC1P;
    TIM2->DIER = TIM_DIER_CC1IE;
#ifdef TOUCH_ZC_GATE_TEST
    TIM2->DIER |= TIM_DIER_CC2IE | TIM_DIER_CC3IE;
    TIM2->CCR2 = 0xFFFFFFFFu;
    TIM2->CCR3 = 0xFFFFFFFFu;
#endif
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    TIM2->CR1 = TIM_CR1_CEN;
    NVIC_SetPriority(TIM2_IRQn, 3);
    Nvic_EnableIRQ(TIM2_IRQn);
#else
    g.Pin  = GPIO_PIN_0;
    g.Mode = GPIO_MODE_IT_FALLING;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
#endif

    g.Pin = GPIO_PIN_0; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP; HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_2; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP; HAL_GPIO_Init(GPIOC, &g);

#ifdef TOUCH_ZC_GATE_TEST
    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI2;
    SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI2_PC;
    EXTI->IMR |= EXTI_IMR_IM2;
    EXTI->FTSR |= EXTI_FTSR_TR2;
    EXTI->RTSR &= ~EXTI_RTSR_TR2;
    EXTI->PR = EXTI_PR_PR2;
    HAL_NVIC_SetPriority(EXTI2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
#endif

#if defined(TOUCH_GATE_TEST) || defined(TOUCH_ZC_GATE_TEST)
    g.Pin = GPIO_PIN_5; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
#endif
}

void EXTI3_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_3) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);
        hall_irq_edges++;
        last_hall_activity_tick = HAL_GetTick();
    }
}

#ifndef TOUCH_ZC_STAGE3
void EXTI0_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_0) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
        zc_irq_edges++;
        zc_capture_period_us = 10000u;
    }
}
#endif

#ifdef TOACH_ZC_GATE_TEST
void EXTI2_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR2) {
        EXTI->PR = EXTI_PR_PR2;
        emergency_latched = 1u;
        ui_running = 0u;
        gate_allow = 0u;
        GPIOA->BSRR = (uint32_t)GPIO_PIN_5 << 16;
        TIM2->CCR2 = 0xFFFFFFFFu;
        TIM2->CCR3 = 0xFFFFFFFFu;
        TIM2->SR &= ~(TIM_SR_CC2IF | TIM_SR_CC3IF);
    }
}
#endif

#ifdef TOUCH_ZC_STAGE3
void TIM2_IRQHandler(void)
{
#ifdef TOUCH_ZC_GATE_TEST
    if (TIM2->SR & TIM_SR_CC2IF) {
        if (gate_allow) GPIOA->BSRR = GPIO_PIN_5;
        TIM2->SR &= ~TIM_SR_CC2IF;
    }
    if (TIM2->SR & TIM_SR_CC3IF) {
        GPIOA->BSRR = (uint32_t)GPIO_PIN_5 << 16;
        TIM2->SR &= ~TIM_SR_CC3IF;
    }
#endif
    if (TIM2->SR & TIM_SR_CC1IF) {
        uint32_t now = TIM2->CCR1;
        uint32_t period = now - zc_capture_last;
        zc_capture_last = now;
        zc_irq_edges++;
        if (period >= 8000u && period <= 12000u) {
            zc_capture_period_us = period;
#ifdef TOUCH_ZC_GATE_TEST
            if (gate_allow && speed_percent > 0u) {
                uint32_t delay = 100u + ((1000u - speed_permille) * 89u) / 10u;
                TIM2->CCR2 = now + delay;
                TIM2->CCR3 = now + delay + 200u;
            }
#endif
        }
        TIM2->SR &= ~(TIM_SR_CC1IF | TIM_SR_CC1OF);
    }
}
#endif

static void pot_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_1; g.Mode = GPIO_MODE_ANALOG; g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);
    __HAL_RCC_ADC1_CLK_ENABLE();
    ADC->CCR = 0; ADC1->CR1 = 0; ADC1->CR2 = ADC_CR2_ADON;
    ADC1->SMPR2 |= ADC_SMPR2_SMP1; ADC1->SQR1 = 0; ADC1->SQR3 = 1;
}

static uint32_t pot_read(void)
{
    ADC1->CR2 |= ADC_CR2_SWSTART;
    uint32_t guard = 50000u;
    while (!(ADC1->SR & ADC_SR_EOC) && --guard) {}
    return guard ? (ADC1->DR & 0xfffu) : 0xffffu;
}

static int pressed(void)
{
    return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5) == GPIO_PIN_RESET;
}

static void draw_ui_layout(void)
{
    ILI9341_FillRectangle(0, 0, 240, 320, C_BG);
    ILI9341_FillRectangle(0, 0, 240, 26, C_PANEL);
    ILI9341_DrawString(18, 5, "SMARTDRILL OS  v5.2", C_CYAN, C_PANEL);

    ILI9341_FillRectangle(10, 34, 105, 50, C_GREEN);
    ILI9341_DrawString(38, 51, "START", C_BG, C_GREEN);

    ILI9341_FillRectangle(125, 34, 105, 50, C_RED);
    ILI9341_DrawString(158, 51, "STOP", C_WHITE, C_RED);

    ILI9341_FillRectangle(10, 92, 220, 56, C_PANEL);
    ILI9341_DrawString(18, 98, "SET SPEED:", C_WHITE, C_PANEL);
    ILI9341_FillRectangle(18, 122, 204, 16, C_BG);

    ILI9341_FillRectangle(10, 156, 220, 54, C_PANEL);
    ILI9341_DrawString(18, 164, "MOTOR RPM :", C_WHITE, C_PANEL);
    ILI9341_DrawString(18, 186, "HALL / AC :", C_WHITE, C_PANEL);

    ILI9341_FillRectangle(10, 218, 220, 26, C_PANEL);
    ILI9341_FillRectangle(10, 252, 220, 38, C_PANEL);
    ILI9341_DrawString(14, 298, "HW S:0 T:0  |  READY", C_BORDER, C_BG);
}

static void update_ui_fast(uint32_t hw_start, uint32_t hw_stop, uint8_t force)
{
    char buf[32];

    if (force || speed_percent != prev_speed) {
        prev_speed = speed_percent;
        uint16_t color = C_GREEN;
        if (speed_percent > 80u) color = C_RED;
        else if (speed_percent > 50u) color = C_YELLOW;

        snprintf(buf, sizeof buf, "%3lu%%", (unsigned long)speed_percent);
        ILI9341_DrawString(155, 98, buf, color, C_PANEL);

        uint16_t fill = (uint16_t)((200u * speed_percent) / 100u);
        if (fill > 0u) ILI9341_FillRectangle(20, 124, fill, 12, color);
        if (fill < 200u) ILI9341_FillRectangle(20u + fill, 124, 200u - fill, 12, C_BG);
    }

    if (force || motor_rpm != prev_rpm) {
        prev_rpm = motor_rpm;
        snprintf(buf, sizeof buf, "%5lu RPM", (unsigned long)motor_rpm);
        ILI9341_DrawString(125, 164, buf, C_CYAN, C_PANEL);
    }

    if (force || hall_rate_hz != prev_hz || zc_rate_hz != prev_zc_hz) {
        prev_hz = hall_rate_hz;
        prev_zc_hz = zc_rate_hz;
        snprintf(buf, sizeof buf, "%3lu/%3luHz", (unsigned long)hall_rate_hz, (unsigned long)zc_rate_hz);
        ILI9341_DrawString(125, 186, buf, C_YELLOW, C_PANEL);
    }

    if (force || ui_running != prev_running) {
        prev_running = ui_running;
        if (ui_running) {
            ILI9341_FillRectangle(10, 218, 220, 26, C_GREEN);
            ILI9341_DrawString(55, 223, "STATE: RUNNING", C_BG, C_GREEN);
        } else {
            ILI9341_FillRectangle(10, 218, 220, 26, C_PANEL);
            ILI9341_DrawString(55, 223, "STATE: STOPPED", C_WHITE, C_PANEL);
        }
    }

    if (force || hw_start != prev_hw_s || hw_stop != prev_hw_t) {
        prev_hw_s = hw_start;
        prev_hw_t = hw_stop;
        snprintf(buf, sizeof buf, "HW S:%lu T:%lu", (unsigned long)hw_start, (unsigned long)hw_stop);
        ILI9341_DrawString(14, 298, buf, C_WHITE, C_BG);
    }

    uint32_t run_active = (run_start_tick != 0u) && (HAL_GetTick() - run_start_tick >= 1500u);
    uint32_t err = 0u;

    if (emergency_latched) err = 1u;
    else if (pot_fault) err = 5u;
    else if (run_active && sensor_disconnected) err = 4u;
    else if (run_active && zc_rate_hz < 40u) err = 2u;
    else if (run_active && speed_percent > 10u && (HAL_GetTick() - last_hall_activity_tick > 1200u)) err = 3u;

    if (force || err != prev_err) {
        prev_err = err;
        switch (err) {
            case 1u:
                ILI9341_FillRectangle(10, 252, 220, 38, C_RED);
                ILI9341_DrawString(24, 263, "ERR E01: EMERGENCY%", C_WHITE, C_RED);
                break;
            case 2u:
                ILI9341_FillRectangle(10, 252, 220, 38, C_RED);
                ILI9341_DrawString(22, 263, "ERR E02: NO ZC SIGNAL", C_WHITE, C_RED);
                break;
            case 3u:
                ILI9341_FillRectangle(10, 252, 220, 38, C_ORANGE);
                ILI9341_DrawString(22, 263, "ERR E03: NO HALL RPM", C_BG, C_ORANGE);
                break;
            case 4u:
                ILI9341_FillRectangle(10, 252, 220, 38, C_ORANGE);
                ILI9341_DrawString(22, 263, "ERR E04: SENSOR OPEN", C_BG, C_ORANGE);
                break;
            case 5u:
                ILI9341_FillRectangle(10, 252, 220, 38, C_RED);
                ILI9341_DrawString(24, 263, "ERR E05: POT ADC ERR", C_WHITE, C_RED);
                break;
            default:
                ILI9341_FillRectangle(10, 252, 220, 38, C_PANEL);
                ILI9341_DrawString(48, 263, "SYSTEM READY: OK", C_GREEN, C_PANEL);
                break;
        }
    }
}

static float absf(float v) { return v < 0 ? -v : v; }

static int fit(void)
{
    float x0 = cal_raw_x[0], y0 = cal_raw_y[0];
    float dx1 = cal_raw_x[1]-x0, dy1 = cal_raw_y[1]-y0;
    float dx2 = cal_raw_x[2]-x0, dy2 = cal_raw_y[2]-y0;
    float det = dx1*dy2 - dx2*dy1;
    if (absf(det) < 1000000.0f) return 0;
    ax = 199.0f*dy2/det; bx = -199.0f*dx2/det;
    ay = -279.0f*dy1/det; by = 279.0f*dx1/det;
    cx = 20.0f-ax*x0-bx*y0; cy = 20.0f-ay*x0-by*y0;
    for (unsigned i = 3; i < 5; i++) {
        float x = ax*cal_raw_x[i]+bx*cal_raw_y[i]+cx;
        float y = ay*cal_raw_x[i]+by*cal_raw_y[i]+cy;
        if (absf(x-px[i]) > 15.0f || absf(y-py[i]) > 15.0f) return 0;
    }
    return 1;
}

void touch_ui_run(void)
{
    boot_stage = 7;
    cal_raw_x[0] = TOUCH_TL_X; cal_raw_y[0] = TOUCH_TL_Y;
    cal_raw_x[1] = TOUCH_TR_X; cal_raw_y[1] = TOUCH_TR_Y;
    cal_raw_x[2] = TOUCH_BL_X; cal_raw_y[2] = TOUCH_BL_Y;
    cal_raw_x[3] = TOUCH_BR_X; cal_raw_y[3] = TOUCH_BR_Y;
    cal_raw_x[4] = TOUCH_CENTER_X; cal_raw_y[4] = TOUCH_CENTER_Y;
    cal_valid = (uint32_t)fit();

    if (!cal_valid) {
        ILI9341_FillRectangle(0, 0, 240, 320, C_BG);
        ILI9341_DrawString(20, 60, "CAL DATA ERROR", C_RED, C_BG);
        HAL_Delay(1000);
    }

    pot_init();
    inputs_init();

    draw_ui_layout();
    update_ui_fast(0u, 0u, 1u);

    boot_stage = 8;
    uint32_t ui_tick = 0;
    uint32_t rate_tick = HAL_GetTick();
    uint32_t hall_last = 0;
    uint32_t zc_last = 0;
    uint8_t touch_lock = 0;

    for (;;) {
        uint32_t now = HAL_GetTick();

        uint32_t hw_start = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
        uint32_t hw_stop  = !HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_2);
        uint8_t was_running = ui_running;

        if (hw_stop) {
            emergency_latched = 1u;
            ui_running = 0u;
        } else if (hw_start && !last_hw_start) {
            emergency_latched = 0u;
            ui_running = 1u;
        }
        last_hw_start = hw_start;

        if (pressed()) {
            if (!touch_lock) {
                uint32_t errors = touch_errors;
                uint16_t x = touch_read(0xD0);
                uint16_t y = touch_read(0x90);
                if (touch_errors == errors && pressed()) {
                    touch_lock = 1u;
                    int mx = (int)(ax * (float)x + bx * (float)y + cx + 0.5f);
                    int my = (int)(ay * (float)x + by * (float)y + cy + 0.5f);
                    if (my >= 30 && my <= 90) {
                        if (mx < 118 && !hw_stop) {
                            emergency_latched = 0u;
                            ui_running = 1u;
                        } else if (mx >= 122) {
                            ui_running = 0u;
                        }
                    }
                }
            }
        } else {
            touch_lock = 0u;
        }

        if (!was_running && ui_running) run_start_tick = now;
        if (!ui_running) run_start_tick = 0u;

        pot_value = pot_read();
        pot_fault = (pot_value > 4095u) ? 1u : 0u;
        if (pot_fault) {
            speed_percent = 0u;
            speed_permille = 0u;
        } else {
            if (pot_filtered == 0u) pot_filtered = pot_value;
            else pot_filtered = (pot_filtered + pot_value) >> 1;

            speed_permille = (pot_filtered * 1000u + 2047u) / 4095u;
            if (speed_permille > 1000u) speed_permille = 1000u;
            speed_percent = (speed_permille + 5u) / 10u;
            if (speed_percent > 100u) speed_percent = 100u;
        }

        gate_allow = (ui_running && !hw_stop && !emergency_latched && speed_permille > 0u) ? 1u : 0u;
#if defined(TOUCH_GATE_TEST) && !defined(TOUCH_ZC_GATE_TEST)
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, gate_allow ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef TOUCH_ZC_GATE_TEST
        if (!gate_allow) {
            GPIOA->BSRR = (uint32_t)GPIO_PIN_5 << 16;
            TIM2->CCR2 = 0xFFFFFFFFu;
            TIM2->CCR3 = 0xFFFFFFFFu;
            TIM2->SR &= ~(TIM_SR_CC2IF | TIM_SR_CC3IF);
        }
#endif

        if (now - rate_tick >= 500u) {
            uint32_t current_hall = hall_irq_edges;
            uint32_t current_zc   = zc_irq_edges;

            uint32_t diff_hall = current_hall - hall_last;
            uint32_t diff_zc   = current_zc - zc_last;

            hall_last = current_hall;
            zc_last   = current_zc;

            hall_rate_hz = diff_hall * 2u;
            zc_rate_hz   = diff_zc * 2u;
            motor_rpm    = hall_rate_hz * 60u;

            rate_tick = now;
        }

        if (now - ui_tick >= 40u) {
            ui_tick = now;
            update_ui_fast(hw_start, hw_stop, 0u);
        }
    }
}


