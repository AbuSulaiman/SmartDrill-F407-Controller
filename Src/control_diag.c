/* F407 V33 input commissioning. All motor output remains inhibited. */
#include "main.h"
#include "fsmc.h"
#include "ili9341.h"
#include <stdio.h>
#define COLOR_YELLOW 0xFFE0

volatile uint32_t boot_stage, boot_error;
static uint32_t zc_edges, hall_edges;
static GPIO_PinState last_zc = GPIO_PIN_RESET, last_hall = GPIO_PIN_RESET;

static void gate_off(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIOA->BSRR = (uint32_t)GPIO_PIN_5 << 16;
    GPIO_InitTypeDef g = {0}; g.Pin = GPIO_PIN_5;
    g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
}
static void clock_init(void)
{
    RCC_OscInitTypeDef o={0}; RCC_ClkInitTypeDef c={0};
    o.OscillatorType=RCC_OSCILLATORTYPE_HSI; o.HSIState=RCC_HSI_ON;
    o.HSICalibrationValue=RCC_HSICALIBRATION_DEFAULT; o.PLL.PLLState=RCC_PLL_NONE;
    if(HAL_RCC_OscConfig(&o)!=HAL_OK) Error_Handler();
    c.ClockType=RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource=RCC_SYSCLKSOURCE_HSI; c.AHBCLKDivider=RCC_SYSCLK_DIV1;
    c.APB1CLKDivider=RCC_HCLK_DIV1; c.APB2CLKDivider=RCC_HCLK_DIV1;
    if(HAL_RCC_ClockConfig(&c,FLASH_LATENCY_0)!=HAL_OK) Error_Handler();
}
static void io_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef g={0};
    g.Pin=GPIO_PIN_0|GPIO_PIN_3; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP; g.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA,&g);
    g.Pin=GPIO_PIN_1; g.Mode=GPIO_MODE_ANALOG; g.Pull=GPIO_NOPULL; HAL_GPIO_Init(GPIOA,&g);
    __HAL_RCC_GPIOC_CLK_ENABLE();
    /* PB1 affects the display and PB2 is not exposed. STOP uses exposed PC2. */
    g.Pin=GPIO_PIN_0; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP; HAL_GPIO_Init(GPIOB,&g);
    g.Pin=GPIO_PIN_2; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP; HAL_GPIO_Init(GPIOC,&g);
    gate_off();
    /* V33: leave PA6/PA7 high impedance. Onboard D2/D3 or TFT backlight
     * routing must not be driven by the input diagnostic. */
    g.Pin=GPIO_PIN_6|GPIO_PIN_7; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_NOPULL; HAL_GPIO_Init(GPIOA,&g);
}
static void adc_init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    ADC->CCR = 0;
    ADC1->CR1 = 0;
    ADC1->CR2 = ADC_CR2_ADON;
    ADC1->SMPR2 |= ADC_SMPR2_SMP1;
    ADC1->SQR1 = 0;
    ADC1->SQR3 = 1;
}
static uint32_t pot_read(void)
{
    ADC1->CR2 |= ADC_CR2_SWSTART;
    uint32_t guard=100000;
    while (!(ADC1->SR & ADC_SR_EOC) && --guard) {}
    return guard ? (ADC1->DR & 0xfffu) : 0xffffu;
}
void SysTick_Handler(void){HAL_IncTick();}
void HardFault_Handler(void){Error_Handler();}
void MemManage_Handler(void){Error_Handler();}
void BusFault_Handler(void){Error_Handler();}
void UsageFault_Handler(void){Error_Handler();}
void NMI_Handler(void){Error_Handler();}
int main(void)
{
    SCB->VTOR=FLASH_BASE; __DSB(); __ISB(); gate_off();
    boot_stage=1; HAL_Init(); boot_stage=2; clock_init(); io_init(); adc_init();
    boot_stage=3; MX_FSMC_Init(); boot_stage=4; ILI9341_Init(); boot_stage=5;
    ILI9341_FillRectangle(0,0,240,320,COLOR_BLACK);
    ILI9341_DrawString(10,8,"F407 CONTROL DIAG",COLOR_WHITE,COLOR_BLACK);
    ILI9341_DrawString(10,35,"MOTOR DISABLED",COLOR_RED,COLOR_BLACK);
    uint32_t ui_tick = 0;
    for(;;){
        gate_off();
        GPIO_PinState zc=HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_0), hall=HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_3);
        if(zc!=last_zc){ if(zc==GPIO_PIN_SET) zc_edges++; last_zc=zc; }
        if(hall!=last_hall){ if(hall==GPIO_PIN_SET) hall_edges++; last_hall=hall; }
        uint32_t pot=pot_read(); uint32_t start=!HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_0), stop=!HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_2);
        /* No LED writes in R3: preserve the board's own LED/backlight nets. */
        uint32_t now = HAL_GetTick();
        if (now - ui_tick >= 50u) {
            ui_tick = now;
            char line[28];
            snprintf(line,sizeof line,"POT:%4lu",(unsigned long)pot); ILI9341_DrawString(10,75,line,COLOR_WHITE,COLOR_BLACK);
            snprintf(line,sizeof line,"START:%lu STOP:%lu",(unsigned long)start,(unsigned long)stop); ILI9341_DrawString(10,105,line,COLOR_WHITE,COLOR_BLACK);
            snprintf(line,sizeof line,"ZERO:%lu HALL:%lu",(unsigned long)zc_edges,(unsigned long)hall_edges); ILI9341_DrawString(10,135,line,COLOR_WHITE,COLOR_BLACK);
            snprintf(line,sizeof line,"IN:%u%u  GATE:OFF",(unsigned)zc,(unsigned)hall); ILI9341_DrawString(10,165,line,COLOR_YELLOW,COLOR_BLACK);
        }
        HAL_Delay(1);
    }
}
void Error_Handler(void){gate_off();boot_error=boot_stage;__disable_irq();for(;;){}}
