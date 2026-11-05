/* Rd-03 (安信可雷达) 在野火小智 STM32F103C8T6 上的最小验证。
 *
 *   USART3  PB10 = MCU TX -> 模块 RX
 *           PB11 = MCU RX <- 模块 TX      115200 8N1
 *   USART1  PA9/PA10 = 调试串口（核心板 CH340）
 *
 * 插法：扩展板 通讯接口2（VCC4 跳线拨 3.3V），Rd-03 的 3V3/GND/TX/RX 正好落在
 *       VCC4/GND/B11/B10 上。PB10/PB11 与 OLED 排针接法冲突，但 OLED 插专用座
 *       （PB6/PB7）时可共存。
 *
 * 模块输出是**逐行 ASCII**，一条事件两行：
 *     ON\r\n
 *     Range 35\r\n
 * 所以这里按行收、按行打印，并单独识别 "Range <n>" 提取距离。
 */
#include <stdio.h>
#include <string.h>

#include "stm32f1xx.h"
#include "usart/bsp_usart.h"
#include "rd03/bsp_rd03.h"

void SystemClock_Config(void);

static int parse_range(const char *line, int *cm)
{
    /* accepts "Range 35" / "range 35" */
    if ((line[0] == 'R' || line[0] == 'r') && strncmp(line + 1, "ange", 4) == 0) {
        const char *p = line + 5;
        int v = 0, any = 0;
        while (*p == ' ') p++;
        while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; any = 1; }
        if (any) { *cm = v; return 1; }
    }
    return 0;
}

int main(void)
{
    int cm = -1;
    int have_range = 0;

    HAL_Init();
    SystemClock_Config();
    USARTX_Init();

    USARTX_SendString(DEBUG_USARTX, (uint8_t*)"\r\n== Rd-03 test (USART3 PB10/PB11 @115200) ==\r\n");

    while (1) {
        if (rd03_receive_data.flag) {
            char line[64];
            uint32_t n = rd03_receive_data.len;
            if (n > sizeof(line) - 1) n = sizeof(line) - 1;
            memcpy(line, rd03_receive_data.data, n);
            line[n] = 0;

            /* strip CR/LF */
            while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = 0;

            /* 原始字节：协议对不上时靠它定位 */
            printf("RAW[%u]:", (unsigned)n);
            for (uint32_t i = 0; i < n && i < 24; i++) printf(" %02X", (uint8_t)line[i]);
            printf("\r\n");

            if (n > 0) {
                if (parse_range(line, &cm)) {
                    have_range = 1;
                    printf("range = %d cm\r\n", cm);
                } else if (strcmp(line, "ON") == 0) {
                    printf("--- detect: ON ---\r\n");
                } else if (strcmp(line, "OFF") == 0) {
                    have_range = 0;
                    printf("--- detect: OFF ---\r\n");
                } else {
                    printf("line: %s\r\n", line);
                }
            }
            Rd03_buff_Reset(&rd03_receive_data);
        }
        (void)have_range;
    }
}

void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef clk = {0};
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { while (1) { } }
    clk.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) { while (1) { } }
}

void Error_Handler(void) { while (1) { } }