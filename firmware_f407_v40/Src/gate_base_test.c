/* Low-voltage Q2 base test. PA5 is HIGH only while START is held and STOP is clear. */
#include "main.h"
#include "fsmc.h"
#include "ili9341.h"
#include <stdio.h>

#define COLOR_WHITE 0xFFFF
#define COLOR_BLACK 0x0000
#define COLOR_GREEN 0x07E0
#define COLOR_RED   0xF800

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
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef g={0};
    g.Pin=GPIO_PIN_5; g.Mode=GPIO_MODE_OUTPUT_PP; g.Pull=GPIO_NOPULL; g.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA,&g);
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5,GPIO_PIN_RESET);
    g.Pin=GPIO_PIN_0; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP; HAL_GPIO_Init(GPIOB,&g);
    g.Pin=GPIO_PIN_2; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP; HAL_GPIO_Init(GPIOC,&g);
}

void SysTick_Handler(void){HAL_IncTick();}
void HardFault_Handler(void){Error_Handler();}
void MemManage_Handler(void){Error_Handler();}
void BusFault_Handler(void){Error_Handler();}
void UsageFault_Handler(void){Error_Handler();}
void NMI_Handler(void){Error_Handler();}

int main(void)
{
    SCB->VTOR=FLASH_BASE; __DSB(); __ISB();
    HAL_Init(); clock_init(); io_init(); MX_FSMC_Init(); ILI9341_Init();
    ILI9341_FillRectangle(0,0,240,320,COLOR_BLACK);
    ILI9341_DrawString(10,10,"Q2 BASE TEST",COLOR_WHITE,COLOR_BLACK);
    ILI9341_DrawString(10,40,"MOTOR DISABLED",COLOR_RED,COLOR_BLACK);
    ILI9341_DrawString(10,70,"HOLD START TO DRIVE PA5",COLOR_WHITE,COLOR_BLACK);
    uint32_t tick=0;
    for(;;){
        /* START is active-low; STOP is active-low. */
        GPIO_PinState start = HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_0);
        GPIO_PinState stop  = HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_2);
        uint32_t q2_on = (start==GPIO_PIN_RESET && stop==GPIO_PIN_SET);
        if (q2_on)
            HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5,GPIO_PIN_SET);
        else
            HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5,GPIO_PIN_RESET);
        if (HAL_GetTick()-tick >= 100u) {
            tick=HAL_GetTick();
            char line[32];
            snprintf(line,sizeof line,"START:%u STOP:%u",(unsigned)(start==GPIO_PIN_RESET),(unsigned)(stop==GPIO_PIN_RESET));
            ILI9341_DrawString(10,120,line,COLOR_WHITE,COLOR_BLACK);
            snprintf(line,sizeof line,"PA5/Q2:%s",q2_on?"ON ":"OFF");
            ILI9341_DrawString(10,150,line,q2_on?COLOR_GREEN:COLOR_WHITE,COLOR_BLACK);
        }
        HAL_Delay(2);
    }
}

void Error_Handler(void)
{
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5,GPIO_PIN_RESET);
    __disable_irq(); for(;;){}
}
