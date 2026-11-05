#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "stm32f1xx.h"

#define DATA_SIZE   2048

/* Same shape the vendor driver expects: it only reads .data / .len / .flag. */
typedef struct {
    volatile uint32_t flag;
    volatile uint32_t len;
    uint8_t data[DATA_SIZE];
} USART_DataTypeDef;

extern USART_DataTypeDef debug_receive_data;
extern USART_DataTypeDef rd03_receive_data;

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;

#define DEBUG_USARTX        USART1
#define RD03_USARTX         USART3
#define DEBUG_BAUDRATE      115200
#define RD03_BAUDRATE       115200

void USARTX_Init(void);
void USARTX_SendString(USART_TypeDef *pusartx, uint8_t *str);
void USARTX_SendString_Lenth(USART_TypeDef *pusartx, uint8_t *str, uint32_t num);

#endif
