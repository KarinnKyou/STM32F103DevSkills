#include "rd03.h"
#include <string.h>

UART_HandleTypeDef huart3;

volatile int      g_radar_present = -1;
volatile int      g_radar_cm      = -1;
volatile uint32_t g_radar_frames;

static uint8_t rx_byte;
static char    linebuf[48];
static uint8_t linelen;

static int starts_with(const char *s, const char *p)
{
    while (*p) {
        if (*s++ != *p++) {
            return 0;
        }
    }
    return 1;
}

static void handle_line(char *line)
{
    g_radar_frames++;

    if (strcmp(line, "ON") == 0) {
        g_radar_present = 1;
        return;
    }
    if (strcmp(line, "OFF") == 0) {
        g_radar_present = 0;
        g_radar_cm = -1;
        return;
    }
    if (starts_with(line, "Range")) {
        const char *p = line + 5;
        int v = 0, any = 0;

        while (*p == ' ') p++;
        while (*p >= '0' && *p <= '9') {
            v = v * 10 + (*p - '0');
            p++;
            any = 1;
        }
        if (any) {
            g_radar_cm = v;
            g_radar_present = 1;
        }
    }
}

void rd03_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_10;              /* USART3_TX */
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_11;              /* USART3_RX */
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);

    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);

    HAL_NVIC_SetPriority(USART3_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
}

void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart3);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
    if (h->Instance == USART3) {
        if (rx_byte == '\n') {
            linebuf[linelen] = 0;
            while (linelen > 0 && (linebuf[linelen - 1] == '\r')) {
                linebuf[--linelen] = 0;
            }
            if (linelen > 0) {
                handle_line(linebuf);
            }
            linelen = 0;
        } else if (linelen < sizeof(linebuf) - 1) {
            linebuf[linelen++] = (char)rx_byte;
        }
        HAL_UART_Receive_IT(&huart3, &rx_byte, 1);   /* re-arm, or it stops after one byte */
    }
}

/* DHT11 的读取是 ~5 ms 的阻塞位操作，期间 USART3 可能溢出。
 * HAL 在溢出后不会自动重启接收，必须在这里重新武装，否则雷达从此静默。 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    if (h->Instance == USART3) {
        linelen = 0;
        HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    }
}