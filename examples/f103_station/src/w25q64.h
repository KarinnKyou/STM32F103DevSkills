#ifndef __W25Q64_H
#define __W25Q64_H

#include "stm32f1xx.h"

/* 软件 SPI 接在扩展板「核心板IO-排针」上，按模块丝印顺序 CS, DO, DI, CLK 连续四位：
 *   CS  -> PA15
 *   DO  -> PB3   (MISO)
 *   DI  -> PB4   (MOSI)
 *   CLK -> PB5
 * PA15/PB3/PB4 是 JTAG 脚，w25q64_init() 会关掉 JTAG（保留 SWD）。
 */
#define W25Q_CS_PORT    GPIOA
#define W25Q_CS_PIN     GPIO_PIN_15
#define W25Q_MISO_PORT  GPIOB
#define W25Q_MISO_PIN   GPIO_PIN_3
#define W25Q_MOSI_PORT  GPIOB
#define W25Q_MOSI_PIN   GPIO_PIN_4
#define W25Q_SCK_PORT   GPIOB
#define W25Q_SCK_PIN    GPIO_PIN_5

int      w25q64_init(void);
uint32_t w25q64_read_id(void);
void     w25q64_read(uint32_t addr, void *dst, uint32_t len);
int      w25q64_page_program(uint32_t addr, const void *src, uint32_t len);
int      w25q64_sector_erase(uint32_t addr);

#endif