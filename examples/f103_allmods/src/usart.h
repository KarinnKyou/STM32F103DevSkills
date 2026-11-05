#ifndef __USART_H
#define __USART_H

#include "stm32f1xx_hal.h"

extern UART_HandleTypeDef huart1;

void usart_init(void);

#endif /* __USART_H */
