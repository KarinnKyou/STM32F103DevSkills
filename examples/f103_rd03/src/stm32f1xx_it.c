#include "stm32f1xx_hal.h"
#include "usart/bsp_usart.h"

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart3);
}
