/* SmartDrill F407 V4.0 - 168MHz High-Speed System Core */
#include "main.h"
#include "fsmc.h"
#include "ili9341.h"
#include "spi.h"
#include <stdio.h>

volatile uint32_t boot_stage;
volatile uint32_t boot_error;
volatile uint32_t touch_pressed, touch_raw_x, touch_raw_y, touch_errors;

void touch_ui_run(void) __attribute__((noreturn));

uint16_t touch_read(uint8_t command)
{
    uint8_t tx[3] = {command, 0, 0}, rx[3] = {0};
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3, 5);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    if (status != HAL_OK) { touch_errors++; return 0; }
    return (uint16_t)((((uint16_t)rx[1] << 8) | rx[2]) >> 3) & 4095u;
}

static void inhibit_legacy_gate(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    GPIOA->BSRR = (uint32_t)GPIO_PIN_5 << 16;
    GPIOC->BSRR = (uint32_t)GPIO_PIN_3 << 16;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = GPIO_PIN_5;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOC, &g);
}

static void clock_init(void)
{
    RCC_OscInitTypeDef o = {0};
    RCC_ClkInitTypeDef c = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    o.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    o.HSIState = RCC_HSI_ON;
    o.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    o.PLL.PLLState = RCC_PLL_ON;
    o.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    o.PLL.PLLM = 8;
    o.PLL.PLLN = 168;
    o.PLL.PLLP = RCC_PLLP_DIV2;
    o.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&o) != HAL_OK) Error_Handler();

    c.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    c.AHBCLKDivider = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV4;
    c.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&c, FLASH_LATENCY_5) != HAL_OK) Error_Handler();
}

void SysTick_Handler(void) { HAL_IncTick(); }
void HardFault_Handler(void) { Error_Handler(); }
void MemManage_Handler(void) { Error_Handler(); }
void BusFault_Handler(void) { Error_Handler(); }
void UsageFault_Handler(void) { Error_Handler(); }
void NMI_Handler(void) { Error_Handler(); }

int main(void)
{
    SCB->VTOR = FLASH_BASE;
    __DSB(); __ISB();
    inhibit_legacy_gate();
    boot_stage = 1;
    HAL_Init();
    boot_stage = 2;
    clock_init();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    g.Pin = GPIO_PIN_12; g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);

    g.Pin = GPIO_PIN_5; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &g);

    MX_SPI2_Init();
    HAL_NVIC_DisableIRQ(SPI2_IRQn);
    boot_stage = 3;
    MX_FSMC_Init();
    boot_stage = 4;
    ILI9341_Init();
    touch_ui_run();
}

void Error_Handler(void)
{
    inhibit_legacy_gate();
    boot_error = boot_stage;
    __disable_irq();
    for (;;) {}
}
