/* Interrupt vector implementations.  The startup file only provides weak
 * aliases, so a strong definition here is enough. */
#include "stm32f1xx_hal.h"
#include "app.h"

void NMI_Handler(void)        { }
void HardFault_Handler(void)  { while (1) { } }
void MemManage_Handler(void)  { while (1) { } }
void BusFault_Handler(void)   { while (1) { } }
void UsageFault_Handler(void) { while (1) { } }
void SVC_Handler(void)        { }
void DebugMon_Handler(void)   { }
void PendSV_Handler(void)     { }

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* 1 kHz: sample the EC11 quadrature lines and debounce its push button. */
void TIM2_IRQHandler(void)
{
    if ((TIM2->SR & TIM_SR_UIF) != 0U) {
        TIM2->SR = (uint32_t)~TIM_SR_UIF;
        app_tim2_tick();
    }
}
