/* HAL transport layer replacing the vendor's StdPeriph bsp_usart.c.
 *
 *   USART1 (PA9/PA10)  = debug console to the PC
 *   USART3 (PB10/PB11) = Rd-03 radar
 *
 * The vendor's bsp_rd03.c only needs USART_DataTypeDef, the two buffers and
 * USARTX_SendString*, so its parsing code is reused unchanged.
 */
#include "usart/bsp_usart.h"
#include <string.h>

USART_DataTypeDef debug_receive_data;
USART_DataTypeDef rd03_receive_data;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

static uint8_t rx_byte1, rx_byte3;

static void USART1_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = DEBUG_BAUDRATE;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);

    HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_UART_Receive_IT(&huart1, &rx_byte1, 1);
}

static void USART3_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);

    huart3.Instance = USART3;
    huart3.Init.BaudRate = RD03_BAUDRATE;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);

    HAL_NVIC_SetPriority(USART3_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    HAL_UART_Receive_IT(&huart3, &rx_byte3, 1);
}

void USARTX_Init(void)
{
    USART1_Init();
    USART3_Init();
}

static void push_byte(USART_DataTypeDef *buf, uint8_t b)
{
    if (buf->len < DATA_SIZE - 1) {
        buf->data[buf->len++] = b;
        buf->data[buf->len] = 0;
        if (b == '\n') {
            buf->flag = 1;          /* line-based frame for the radar */
        }
    } else {
        buf->len = 0;
        buf->data[0] = 0;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        push_byte(&debug_receive_data, rx_byte1);
        HAL_UART_Receive_IT(&huart1, &rx_byte1, 1);
    } else if (huart->Instance == USART3) {
        push_byte(&rd03_receive_data, rx_byte3);
        HAL_UART_Receive_IT(&huart3, &rx_byte3, 1);
    }
}

static UART_HandleTypeDef *handle_of(USART_TypeDef *pusartx)
{
    if (pusartx == USART1) return &huart1;
    if (pusartx == USART3) return &huart3;
    return 0;
}

void USARTX_SendString_Lenth(USART_TypeDef *pusartx, uint8_t *str, uint32_t num)
{
    UART_HandleTypeDef *h = handle_of(pusartx);
    if (h) {
        HAL_UART_Transmit(h, str, (uint16_t)num, 0x1000);
    }
}

void USARTX_SendString(USART_TypeDef *pusartx, uint8_t *str)
{
    USARTX_SendString_Lenth(pusartx, str, (uint32_t)strlen((char *)str));
}
