/* Minimal SSD1306 128x64 driver over a bit-banged I2C bus.
 *
 * The bus is declared at run time (port + SCL/SDA pins), so the same binary
 * can talk to an OLED on PB10/PB11 or PB6/PB7 depending on where the module
 * actually is; main() probes both and picks whichever answers.
 */
#ifndef __OLED_H
#define __OLED_H

#include "stm32f1xx_hal.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      scl;
    uint16_t      sda;
} oled_bus_t;

/* 0x3C is the usual 7-bit address (write byte 0x78). */
#define OLED_ADDR_DEFAULT   0x3C

int  soft_i2c_probe(GPIO_TypeDef *port, uint16_t scl, uint16_t sda, uint8_t addr7);
int  oled_init(const oled_bus_t *bus, uint8_t addr7);   /* 1 = ok */
void oled_clear(void);
void oled_fill(uint8_t pattern);
void oled_all_on(int on);
void oled_line(uint8_t page, const char *fmt, ...);
void oled_bar(uint8_t page, uint16_t permille);
void oled_bus_name(char *out, unsigned len);

#endif /* __OLED_H */
