#include "main.h"
#include "drill_config.h"
#include <stdio.h>

ADC_HandleTypeDef hadc;
volatile uint32_t running, fault, rpm, hall_last_ms, hall_period_us;
volatile uint32_t zc_last_us, zc_last_ms, zc_good, command_percent;
static volatile uint32_t zc_raw_count, zc_raw_us, zc_raw_ms, zc_raw_period;
static volatile uint32_t phase;
/* Software timing telemetry: not an independent measurement of the pin. */
static volatile uint32_t gate_requested_us, gate_actual_us;
static volatile uint32_t started_ms;
static volatile uint32_t drive_percent, control_last_ms;
static volatile uint32_t have_hall, last_hall_us;
static uint8_t lcd_online;
static uint8_t lcd_address = LCD_ADDRESS_7BIT;
volatile uint32_t boot_stage;
static uint32_t reset_flags;
static volatile uint32_t sync_cause, sync_value_us, sync_output;

/* Read tick and ISR-owned timestamp together: an ISR must not publish a
 * timestamp newer than the foreground tick used for unsigned subtraction. */
static uint32_t age_ms(volatile uint32_t *stamp)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    uint32_t age = HAL_GetTick() - *stamp;
    __set_PRIMASK(mask);
    return age;
}

void Error_Handler(void);
static void gate_off(void) { GPIOA->BSRR = (uint32_t)GPIO_PIN_5 << 16; }
static void stop_motor(uint32_t reason)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    running = 0; fault = reason; phase = 0;
    drive_percent = 0;
    TIM2->DIER = 0; TIM2->SR = 0; gate_off();
    __set_PRIMASK(mask);
}
static void stop_sync(uint32_t cause, uint32_t value_us)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    /* Latch the first event before cancelling the output. Foreground LCD
     * reads it only after running=0, so later healthy edges cannot erase it. */
    if (running) {
        sync_cause = cause; sync_value_us = value_us;
        sync_output = drive_percent;
        stop_motor(1);
    }
    __set_PRIMASK(mask);
}
static void clock_init(void)
{
    RCC_OscInitTypeDef o = {0}; RCC_ClkInitTypeDef c = {0};
    o.OscillatorType = RCC_OSCILLATORTYPE_HSI; o.HSIState = RCC_HSI_ON;
    o.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    o.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&o) != HAL_OK) Error_Handler();
    c.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource = RCC_SYSCLKSOURCE_HSI; c.AHBCLKDivider = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV1; c.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&c, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
}
static void gpio_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    gate_off();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
    g.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_1; g.Mode = GPIO_MODE_ANALOG; HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &g);
#if GATE_OUTPUT_ACTIVE
    g.Pin = GPIO_PIN_1; g.Mode = GPIO_MODE_IT_FALLING;
    HAL_GPIO_Init(GPIOB, &g);
    HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0); HAL_NVIC_EnableIRQ(EXTI1_IRQn);
#endif
    g.Pin = GPIO_PIN_0; g.Mode = GPIO_MODE_IT_FALLING; g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_3; g.Mode = GPIO_MODE_IT_RISING; HAL_GPIO_Init(GPIOA, &g);
    g.Mode = GPIO_MODE_OUTPUT_OD; g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = GPIO_PIN_15; HAL_GPIO_WritePin(GPIOA, g.Pin, GPIO_PIN_SET); HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_7; HAL_GPIO_WritePin(GPIOB, g.Pin, GPIO_PIN_SET); HAL_GPIO_Init(GPIOB, &g);
    HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0); HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    HAL_NVIC_SetPriority(EXTI3_IRQn, 2, 0); HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}
static void timer_init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM2->PSC = 15; TIM2->ARR = 0xffffffffu; TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0; TIM2->CR1 = TIM_CR1_CEN;
    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0); HAL_NVIC_EnableIRQ(TIM2_IRQn);
}
void EXTI0_IRQHandler(void)
{
    if (!(EXTI->PR1 & GPIO_PIN_0)) return;
    EXTI->PR1 = GPIO_PIN_0;
    uint32_t now = TIM2->CNT, interval = now - zc_last_us;
    if (zc_raw_count) zc_raw_period = now - zc_raw_us;
    zc_raw_us = now; zc_raw_ms = HAL_GetTick(); zc_raw_count++;
    /* Reject closely spaced noise without replacing the valid reference. */
    if (zc_last_us && interval < 7000u) return;
    gate_off(); TIM2->DIER = 0; TIM2->SR = 0; phase = 0;
    if (zc_last_us && interval >= 9000u && interval <= 11000u) {
        if (zc_good < 20) zc_good++;
    } else {
        zc_good = 0;
        if (running && GATE_OUTPUT_ACTIVE) stop_sync(1, interval);
    }
    zc_last_us = now; zc_last_ms = HAL_GetTick();
    if (!running || !command_percent || zc_good < 5) return;
#if MOTOR_OUTPUT_ENABLED
    if (HAL_GetTick() - control_last_ms > FOREGROUND_TIMEOUT_MS) { stop_motor(6); return; }
    uint32_t output = drive_percent;
    if (!output) return;
#else
    uint32_t output = command_percent;
#endif
    uint32_t delay = 9000u - output * 80u;
    delay = delay > ZERO_EDGE_OFFSET_US ? delay - ZERO_EDGE_OFFSET_US : 100u;
    TIM2->CCR1 = now + delay;
    gate_requested_us = delay;
    phase = 1;
#if GATE_OUTPUT_ACTIVE
    /* Hardware-timer timeout cancels the request even if foreground LCD stalls. */
    TIM2->CCR2 = now + 25000u;
    TIM2->DIER = TIM_DIER_CC1IE | TIM_DIER_CC2IE;
#else
    TIM2->DIER = TIM_DIER_CC1IE;
#endif
}
void EXTI1_IRQHandler(void)
{
    if (EXTI->PR1 & GPIO_PIN_1) {
        EXTI->PR1 = GPIO_PIN_1;
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET) stop_motor(0);
    }
}
static void timer_service(void)
{
    uint32_t pending = TIM2->SR & TIM2->DIER;
    if (pending & TIM_SR_CC2IF) { zc_good = 0; stop_sync(2, TIM2->CNT - zc_last_us); return; }
    if (!(pending & TIM_SR_CC1IF)) return;
    TIM2->SR = ~TIM_SR_CC1IF;
    if (!running || HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET) {
        stop_motor(0); return;
    }
    if (phase == 1) {
#if GATE_OUTPUT_ACTIVE
        if (!command_percent || TIM2->CNT - zc_last_us > 9500u || zc_good < 5u) {
            stop_sync(!command_percent ? 7u : (zc_good < 5u ? 4u : 3u), TIM2->CNT - zc_last_us); return;
        }
        GPIOA->BSRR = GPIO_PIN_5;
        gate_actual_us = TIM2->CNT - zc_last_us;
#endif
        phase = 2; TIM2->CCR1 = TIM2->CNT + GATE_PULSE_US;
    } else {
        gate_off(); phase = 0;
        /* Keep CC2 timeout armed after the end of the pulse. */
        TIM2->DIER &= ~TIM_DIER_CC1IE;
    }
}
void TIM2_IRQHandler(void)
{
    /* STOP or zero-cross must not interrupt between checking running and
     * asserting PA5, then let this handler resume and reassert a stopped gate. */
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    timer_service();
    __set_PRIMASK(mask);
}
void EXTI3_IRQHandler(void)
{
    if (!(EXTI->PR1 & GPIO_PIN_3)) return;
    EXTI->PR1 = GPIO_PIN_3;
    uint32_t now = TIM2->CNT;
    if (have_hall && now - last_hall_us < 2000u) return;
    if (have_hall) {
        hall_period_us = now - last_hall_us;
#if MOTOR_OUTPUT_ENABLED
        if (running && hall_period_us < 60000000u / OVERSPEED_RPM) stop_motor(3);
#endif
    }
    last_hall_us = now; have_hall = 1; hall_last_ms = HAL_GetTick();
}
void SysTick_Handler(void)
{
    HAL_IncTick();
#if MOTOR_OUTPUT_ENABLED
    /* Also cancel on foreground stalls with no dependency on LCD updates. */
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    uint32_t now = HAL_GetTick();
    if (running) {
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET) stop_motor(0);
        else if (now - control_last_ms > FOREGROUND_TIMEOUT_MS) stop_motor(6);
        else if (now - zc_last_ms > 30u) stop_sync(5, TIM2->CNT - zc_last_us);
        else if (now - started_ms > STARTUP_TIMEOUT_MS &&
                 (!have_hall || !hall_period_us || now - hall_last_ms > HALL_TIMEOUT_MS)) stop_motor(2);
    }
    __set_PRIMASK(mask);
#endif
}
void HardFault_Handler(void) { gate_off(); while (1) {} }

static uint8_t adc_init(void)
{
    RCC_PeriphCLKInitTypeDef p = {0};
    p.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
    p.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&p) != HAL_OK) return 0;
    __HAL_RCC_ADC12_CLK_ENABLE();
    hadc.Instance = ADC1;
    hadc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
    hadc.Init.Resolution = ADC_RESOLUTION_12B;
    hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc.Init.NbrOfConversion = 1;
    hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    if (HAL_ADC_Init(&hadc) != HAL_OK) return 0;
    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel = ADC_CHANNEL_2; ch.Rank = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
    ch.SingleDiff = ADC_SINGLE_ENDED; ch.OffsetNumber = ADC_OFFSET_NONE;
    if (HAL_ADC_ConfigChannel(&hadc, &ch) != HAL_OK ||
        HAL_ADCEx_Calibration_Start(&hadc, ADC_SINGLE_ENDED) != HAL_OK) return 0;
    return 1;
}

/* Slow software I2C on PA15/PB7; motor interrupts remain enabled. */
static void us(uint32_t n)
{
    uint32_t t = TIM2->CNT; while ((uint32_t)(TIM2->CNT - t) < n) {}
}
static void scl(uint32_t high) { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void sda(uint32_t high) { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static uint8_t scl_up(void)
{
    scl(1); uint32_t t = TIM2->CNT;
    while (!HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15)) if (TIM2->CNT - t > 1000) return 0;
    us(5); return 1;
}
static uint8_t i2c_byte(uint8_t b)
{
    for (int i = 0; i < 8; i++) {
        scl(0); sda(b & 0x80); us(5);
        if (!scl_up()) return 0;
        scl(0); b <<= 1;
    }
    sda(1); us(5); if (!scl_up()) return 0;
    uint8_t ack = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7); scl(0); return ack;
}
static uint8_t backpack(uint8_t value)
{
    sda(1); if (!scl_up()) return 0; sda(0); us(5); scl(0);
    uint8_t ok = i2c_byte(lcd_address << 1);
    if (ok) ok = i2c_byte(value);
    scl(0); sda(0); us(5); scl_up(); sda(1); us(5);
    return ok;
}
static void nibble(uint8_t n, uint8_t rs)
{
    if (!lcd_online) return;
    uint8_t b = (n & 0xf0) | 8 | rs;
    if (!backpack(b) || !backpack(b | 4) || !backpack(b)) lcd_online = 0;
    us(60);
}
static void lcd_byte(uint8_t b, uint8_t rs) { nibble(b, rs); nibble(b << 4, rs); }
static void lcd_init(void)
{
    HAL_Delay(50); lcd_online = backpack(8); if (!lcd_online) return;
    nibble(0x30, 0); HAL_Delay(5); nibble(0x30, 0); HAL_Delay(1);
    nibble(0x30, 0); nibble(0x20, 0);
    lcd_byte(0x28, 0); lcd_byte(0x0c, 0); lcd_byte(0x06, 0);
    lcd_byte(1, 0); HAL_Delay(3);
}
static void lcd_line(uint8_t row, const char *s)
{
    if (!lcd_online) return;
    lcd_byte(row ? 0xc0 : 0x80, 0);
    uint32_t end = 0;
    for (int i = 0; i < 16; i++) {
        if (!end && !*s) end = 1;
        lcd_byte(end ? ' ' : (uint8_t)*s++, 1);
        if (!lcd_online) break;
    }
}
static uint8_t lcd_detect(void)
{
    /* Recover a backpack left halfway through a transaction by reset. */
    sda(1);
    for (int i = 0; i < 9; i++) { scl(0); us(10); scl_up(); }
    scl(0); sda(0); us(10); scl_up(); sda(1);
    uint8_t found = 0;
    for (uint32_t a = 0x20; a <= 0x3f; a++) {
        if (a > 0x27 && a < 0x38) continue;
        sda(1); if (!scl_up()) break;
        sda(0); us(5); scl(0);
        uint8_t ack = i2c_byte(a << 1);
        scl(0); sda(0); us(5); scl_up(); sda(1); us(5);
        if (ack) { found = a; break; }
    }
    lcd_online = 0;
    if (found) { lcd_address = found; lcd_init(); }
    return found;
}
#ifdef DRILL_DIAGNOSTIC
/* Tests only the low-voltage board. No ADC, zero-cross requirement or motor. */
static void diagnostic(void)
{
    HAL_NVIC_DisableIRQ(EXTI0_IRQn);
    HAL_NVIC_DisableIRQ(EXTI3_IRQn);
    TIM2->DIER = 0;
    for (int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
        HAL_Delay(150);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
        HAL_Delay(150);
    }
    uint8_t found = lcd_detect();
    boot_stage = 10;
    uint32_t last = 0;
    for (;;) {
        uint32_t now = HAL_GetTick(); gate_off();
        uint32_t start = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
        uint32_t stop = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);
        /* Direct button indication, independent of zero-cross and pot. */
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, start ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7,
            (stop || (!lcd_online && (now % 1000u) < 150u)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        if (now - last >= 250) {
            last = now; char line[32];
            snprintf(line, sizeof line, "DIAG I2C:%02X", found); lcd_line(0, line);
            snprintf(line, sizeof line, "S:%lu T:%lu %4lus", (unsigned long)start,
                (unsigned long)stop, (unsigned long)(now / 1000)); lcd_line(1, line);
        }
        HAL_Delay(1);
    }
}
#endif
int main(void)
{
    reset_flags = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;
    /* Explicit flash vector location also supports debugger/bootloader entry. */
    SCB->VTOR = FLASH_BASE; __DSB(); __ISB();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gate_off();
    GPIO_InitTypeDef boot_gpio = {0};
    boot_gpio.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    boot_gpio.Mode = GPIO_MODE_OUTPUT_PP; boot_gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &boot_gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
    boot_stage = 1; HAL_Init();
    boot_stage = 2; clock_init();
    boot_stage = 3; timer_init(); gpio_init();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
#ifdef DRILL_DIAGNOSTIC
    diagnostic();
#endif
    boot_stage = 5; lcd_detect();
#if DRILL_GATE_TEST
    lcd_line(0, "PA5 STABILITY V3");
    lcd_line(1, "2026-10-26 NO AC");
#elif defined(DRILL_SYNC_DIAG)
    lcd_line(0, "G431 SYNC DIAG");
    lcd_line(1, "2026-10-28");
#elif MOTOR_OUTPUT_ENABLED
    lcd_line(0, "G431 MOTOR RC1");
    lcd_line(1, "2026-10-27");
#elif defined(DRILL_ZERO_TEST)
    lcd_line(0, "G431 ZC TEST");
    lcd_line(1, "2026-10-24");
#else
    lcd_line(0, "G431 BENCH V3");
    lcd_line(1, "2026-10-23");
#endif
    HAL_Delay(1200);
    boot_stage = 4; uint8_t adc_ready = adc_init();
    if (!adc_ready) fault = 4;
    boot_stage = 6;
    /* About 2 seconds watchdog with nominal 32 kHz LSI; timeout varies with LSI. */
    IWDG->KR = 0x5555; IWDG->PR = 4; IWDG->RLR = 999;
    IWDG->KR = 0xaaaa; IWDG->KR = 0xcccc;
    uint32_t adc_tick = 0, lcd_tick = 0, filtered = 0, lcd_retry = 0;
    uint32_t start_since = HAL_GetTick(), release_since = HAL_GetTick(), armed = 0;
#if MOTOR_OUTPUT_ENABLED
    uint32_t drive_tick = HAL_GetTick();
#endif
#ifdef DRILL_ZERO_TEST
    uint32_t z_window = HAL_GetTick(), z_previous = zc_raw_count, z_hz = 0;
#endif
    for (;;) {
        uint32_t now = HAL_GetTick(); IWDG->KR = 0xaaaa;
        control_last_ms = now;
        uint32_t start = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
        uint32_t stop = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);
#ifdef DRILL_ZERO_TEST
        /* This test must never arm even a simulated firing sequence. */
        stop = 1;
#endif
        if (stop) { stop_motor(0); armed = 0; release_since = now; start_since = now; }
        else if (!start) {
            start_since = now; if (now - release_since >= 30) armed = 1;
        } else {
            release_since = now;
            if (armed && now - start_since >= 30) {
                armed = 0;
                if (running) continue;
                if (adc_ready && (!GATE_OUTPUT_ACTIVE || command_percent) && (!GATE_OUTPUT_ACTIVE ||
                    (zc_good >= 5 && age_ms(&zc_last_ms) < 30))) {
                    uint32_t mask = __get_PRIMASK(); __disable_irq();
                    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_SET &&
                        (!GATE_OUTPUT_ACTIVE || (HAL_GetTick() - zc_last_ms < 30u && zc_good >= 5u))) {
#if MOTOR_OUTPUT_ENABLED
                        if (rpm > OVERSPEED_RPM) { fault = 3; }
                        else {
                            sync_cause = 0; sync_value_us = 0; sync_output = 0;
                            have_hall = 0; hall_period_us = 0; hall_last_ms = 0;
                            drive_percent = 1; drive_tick = now;
                            control_last_ms = HAL_GetTick();
                            fault = 0; started_ms = HAL_GetTick(); running = 1;
                        }
#else
                        fault = 0; started_ms = now; running = 1;
#endif
                    }
                    __set_PRIMASK(mask);
                } else fault = !adc_ready ? 4u : (!command_percent ? 5u : 1u);
            }
        }
        if (now - adc_tick >= 20) {
            adc_tick = now;
            if (!adc_ready || HAL_ADC_Start(&hadc) != HAL_OK || HAL_ADC_PollForConversion(&hadc, 2) != HAL_OK) {
                if (adc_ready) HAL_ADC_Stop(&hadc);
                stop_motor(4);
            } else {
                uint32_t v = HAL_ADC_GetValue(&hadc); HAL_ADC_Stop(&hadc);
                filtered = (filtered * 3 + v) / 4;
                command_percent = filtered < 80 ? 0 : (filtered * 100u / 4095u);
                if (!command_percent && running && GATE_OUTPUT_ACTIVE) stop_motor(0);
            }
        }
        uint32_t period = hall_period_us;
        rpm = have_hall && age_ms(&hall_last_ms) < HALL_TIMEOUT_MS && period ? 60000000u / period : 0;
        if (running && GATE_OUTPUT_ACTIVE) {
            if (age_ms(&zc_last_ms) > 30) { stop_sync(6, TIM2->CNT - zc_last_us); }
#if MOTOR_OUTPUT_ENABLED
            else if (rpm > OVERSPEED_RPM) stop_motor(3);
            else if (age_ms(&started_ms) > STARTUP_TIMEOUT_MS &&
                     (!have_hall || !period || age_ms(&hall_last_ms) > HALL_TIMEOUT_MS)) stop_motor(2);
            if (running && now - drive_tick >= SOFT_START_STEP_MS) {
                drive_tick = now;
                /* Limit increases to one percentage point/20ms. Decreases
                 * apply immediately at this update; never boost on Hall loss. */
                if (drive_percent > command_percent) drive_percent = command_percent;
                else if (drive_percent < command_percent) drive_percent++;
            }
#endif
        }
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, running ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, running ? GPIO_PIN_RESET : GPIO_PIN_SET);
#ifdef DRILL_ZERO_TEST
        uint32_t z_mask = __get_PRIMASK(); __disable_irq();
        uint32_t z_count = zc_raw_count, z_period = zc_raw_period, z_seen = zc_raw_ms;
        uint32_t z_good = zc_good;
        __set_PRIMASK(z_mask);
        if (now - z_window >= 1000u) {
            z_hz = (uint32_t)((uint64_t)(z_count - z_previous) * 1000u / (now - z_window));
            z_previous = z_count; z_window = now;
        }
        uint32_t z_fresh = z_count && now - z_seen < 100u;
        uint32_t z_ok = z_fresh && now - z_seen < 30u && z_good >= 5u && z_hz >= 98u && z_hz <= 102u;
        gate_off();
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, z_ok ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, z_ok ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
        if (!lcd_online && now - lcd_retry >= 3000 && !running) {
            lcd_retry = now; lcd_detect();
        }
        if (now - lcd_tick >= 250) {
            lcd_tick = now; char line[32];
#if DRILL_GATE_TEST
            snprintf(line, sizeof line, "%s %3lu%% E%lu %c", running ? "RUN " : "STOP", (unsigned long)command_percent, (unsigned long)fault, (now / 1000u) & 1u ? '*' : '.');
            lcd_line(0, line);
            uint32_t timing_mask = __get_PRIMASK();
            __disable_irq();
            uint32_t requested = gate_requested_us, actual = gate_actual_us;
            __set_PRIMASK(timing_mask);
            if ((now / 2000u) & 1u) {
                char reset = reset_flags & RCC_CSR_IWDGRSTF ? 'W' :
                    (reset_flags & RCC_CSR_BORRSTF ? 'P' :
                    (reset_flags & RCC_CSR_SFTRSTF ? 'S' : 'R'));
                snprintf(line, sizeof line, "U%5lu B%lu%lu R%c", (unsigned long)((now / 1000u) % 100000u), (unsigned long)start, (unsigned long)stop, reset);
            } else snprintf(line, sizeof line, "S:%4lu A:%4lu", (unsigned long)requested, (unsigned long)actual);
            lcd_line(1, line);
#elif defined(DRILL_ZERO_TEST)
            snprintf(line, sizeof line, "Z:%3luHz %s", (unsigned long)(z_fresh ? (z_hz > 999 ? 999 : z_hz) : 0),
                !z_fresh ? "NO SIG" : (z_ok ? "OK" : "CHECK"));
            lcd_line(0, line);
            snprintf(line, sizeof line, "PA0:%u P:%5lu", (unsigned)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0),
                (unsigned long)(z_fresh ? (z_period > 99999 ? 99999 : z_period) : 0));
            lcd_line(1, line);
#elif MOTOR_OUTPUT_ENABLED
            snprintf(line, sizeof line, "%s %3lu%% E%lu %c", running ? "RUN " : "STOP", (unsigned long)command_percent, (unsigned long)fault, (now / 1000u) & 1u ? '*' : '.');
            lcd_line(0, line);
            if (fault == 1u && sync_cause) {
                snprintf(line, sizeof line, "Z%lu %5lu O%02lu", (unsigned long)sync_cause,
                    (unsigned long)(sync_value_us > 99999u ? 99999u : sync_value_us), (unsigned long)sync_output);
            } else snprintf(line, sizeof line, "RPM:%4lu O:%3lu%%", (unsigned long)(rpm > 9999 ? 9999 : rpm), (unsigned long)drive_percent);
            lcd_line(1, line);
#else
            snprintf(line, sizeof line, "%s %3lu%% E%lu", running ? "RUN " : "STOP", (unsigned long)command_percent, (unsigned long)fault);
            lcd_line(0, line);
            snprintf(line, sizeof line, "RPM:%4lu S%lu T%lu", (unsigned long)(rpm > 9999 ? 9999 : rpm),
                (unsigned long)start, (unsigned long)stop);
            lcd_line(1, line);
#endif
        }
        HAL_Delay(1);
    }
}
void Error_Handler(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE(); gate_off(); __disable_irq(); while (1) {}
}
