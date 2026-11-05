/* Bit-banged I2C + SSD1306 128x64 text driver.
 *
 * Two details matter on STM32F1:
 *  - An open-drain output does NOT get the internal pull-up (on F1 the pull
 *    resistor only exists in input mode), so "release" is implemented by
 *    switching the pin to input-with-pull-up.  Without this, a socket that
 *    has no external pull-ups floats and every address looks like an ACK.
 *  - The bus is declared at run time, so one binary can talk to an OLED on
 *    PB6/PB7 (OLED socket) or PB10/PB11 (comms socket 2).
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "oled.h"
#include "font6x8.h"

#define OLED_PAGES      8
#define OLED_COLS       128
#define TEXT_COLS       21          /* 21 * 6 px = 126 px */

static GPIO_TypeDef *s_port;
static uint16_t      s_scl;
static uint16_t      s_sda;
static uint8_t       s_addr;
static int           s_ready;

static void i2c_delay(void)
{
    /* ~1 us at 72 MHz, on top of the mode-switch cost. */
    for (volatile uint32_t i = 0; i < 10; i++) {
    }
}

/* Release a line: input with the internal pull-up enabled. */
static void line_high(uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(s_port, &gpio);
}

/* Pull a line low: open-drain output driving 0. */
static void line_low(uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(s_port, &gpio);
    HAL_GPIO_WritePin(s_port, pin, GPIO_PIN_RESET);
}

static int line_read(uint16_t pin)
{
    return HAL_GPIO_ReadPin(s_port, pin) == GPIO_PIN_SET;
}

static void i2c_start(void)
{
    line_high(s_sda);
    line_high(s_scl);
    i2c_delay();
    line_low(s_sda);
    i2c_delay();
    line_low(s_scl);
}

static void i2c_stop(void)
{
    line_low(s_sda);
    i2c_delay();
    line_high(s_scl);
    i2c_delay();
    line_high(s_sda);
    i2c_delay();
}

static int i2c_write_byte(uint8_t b)
{
    int i;
    int ack;

    for (i = 7; i >= 0; i--) {
        line_low(s_scl);
        if ((b >> i) & 1) {
            line_high(s_sda);
        } else {
            line_low(s_sda);
        }
        i2c_delay();
        line_high(s_scl);
        i2c_delay();
    }
    /* ninth clock: release SDA and sample the slave's ACK */
    line_low(s_scl);
    line_high(s_sda);
    i2c_delay();
    line_high(s_scl);
    i2c_delay();
    ack = !line_read(s_sda);
    line_low(s_scl);
    return ack;
}

static void bus_init(GPIO_TypeDef *port, uint16_t scl, uint16_t sda)
{
    s_port = port;
    s_scl = scl;
    s_sda = sda;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    line_high(scl);
    line_high(sda);
    i2c_delay();
}

int soft_i2c_probe(GPIO_TypeDef *port, uint16_t scl, uint16_t sda, uint8_t addr7)
{
    int ack;

    bus_init(port, scl, sda);
    i2c_start();
    ack = i2c_write_byte((uint8_t)(addr7 << 1));
    i2c_stop();
    return ack;
}

static void oled_cmd(uint8_t c)
{
    i2c_start();
    if (!i2c_write_byte((uint8_t)(s_addr << 1))) {
        i2c_stop();
        s_ready = 0;
        return;
    }
    i2c_write_byte(0x00);     /* Co = 0, D/C# = 0: command stream */
    i2c_write_byte(c);
    i2c_stop();
}

static void oled_data(const uint8_t *p, unsigned n)
{
    unsigned i;

    i2c_start();
    if (!i2c_write_byte((uint8_t)(s_addr << 1))) {
        i2c_stop();
        s_ready = 0;
        return;
    }
    i2c_write_byte(0x40);     /* Co = 0, D/C# = 1: data stream */
    for (i = 0; i < n; i++) {
        i2c_write_byte(p[i]);
    }
    i2c_stop();
}

static void oled_set_pos(uint8_t page, uint8_t col)
{
    oled_cmd((uint8_t)(0xB0 | (page & 0x07)));
    oled_cmd((uint8_t)(0x00 | (col & 0x0F)));
    oled_cmd((uint8_t)(0x10 | ((col >> 4) & 0x0F)));
}

int oled_init(const oled_bus_t *bus, uint8_t addr7)
{
    static const uint8_t init_seq[] = {
        0xAE,             /* display off            */
        0xD5, 0x80,       /* clock divide / osc     */
        0xA8, 0x3F,       /* multiplex 64           */
        0xD3, 0x00,       /* display offset 0       */
        0x40,             /* start line 0           */
        0x8D, 0x14,       /* charge pump on         */
        0x20, 0x00,       /* horizontal addressing  */
        0xA1,             /* segment remap          */
        0xC8,             /* COM scan direction     */
        0xDA, 0x12,       /* COM pins               */
        0x81, 0xCF,       /* contrast               */
        0xD9, 0xF1,       /* pre-charge             */
        0xDB, 0x40,       /* VCOMH                  */
        0xA4,             /* resume from RAM        */
        0xA6,             /* normal (not inverted)  */
        0x2E,             /* scroll off             */
        0xAF              /* display on             */
    };
    unsigned i;

    bus_init(bus->port, bus->scl, bus->sda);
    if (!soft_i2c_probe(bus->port, bus->scl, bus->sda, addr7)) {
        return 0;
    }
    bus_init(bus->port, bus->scl, bus->sda);

    s_addr = addr7;
    s_ready = 1;
    for (i = 0; i < sizeof(init_seq); i++) {
        oled_cmd(init_seq[i]);
    }
    if (!s_ready) {
        return 0;
    }
    oled_clear();
    return s_ready;
}

void oled_clear(void)
{
    uint8_t zeros[OLED_COLS];
    uint8_t page;

    memset(zeros, 0, sizeof(zeros));
    for (page = 0; page < OLED_PAGES; page++) {
        oled_set_pos(page, 0);
        oled_data(zeros, sizeof(zeros));
    }
}

/* Fill every pixel with `pattern` (0xFF = white, 0x00 = black). */
void oled_fill(uint8_t pattern)
{
    uint8_t row[OLED_COLS];
    uint8_t page;

    memset(row, pattern, sizeof(row));
    for (page = 0; page < OLED_PAGES; page++) {
        oled_set_pos(page, 0);
        oled_data(row, sizeof(row));
    }
}

/* 0xA5 forces all pixels on regardless of RAM contents: a panel-level test
 * that works even if the font/address mode were wrong. */
void oled_all_on(int on)
{
    oled_cmd(on ? 0xA5 : 0xA4);
}

void oled_line(uint8_t page, const char *fmt, ...)
{
    char text[40];
    char fixed[TEXT_COLS + 1];
    uint8_t buf[TEXT_COLS * 6];
    va_list ap;
    int i;

    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);

    for (i = 0; i < TEXT_COLS; i++) {
        char c = text[i];
        if (c == '\0') {
            break;
        }
        fixed[i] = c;
    }
    for (; i < TEXT_COLS; i++) {          /* pad so old glyphs are erased */
        fixed[i] = ' ';
    }
    fixed[TEXT_COLS] = '\0';

    for (i = 0; i < TEXT_COLS; i++) {
        unsigned char c = (unsigned char)fixed[i];
        if (c < FONT6X8_FIRST || c > 0x7A) {
            c = ' ';
        }
        memcpy(&buf[i * 6], F6x8[c - FONT6X8_FIRST], 6);
    }

    oled_set_pos(page, 0);
    oled_data(buf, sizeof(buf));
}

void oled_bar(uint8_t page, uint16_t permille)
{
    uint8_t buf[OLED_COLS];
    unsigned width = 120;
    unsigned filled;
    unsigned i;

    if (permille > 1000) {
        permille = 1000;
    }
    filled = (unsigned)permille * width / 1000;

    memset(buf, 0, sizeof(buf));
    for (i = 0; i < filled; i++) {
        buf[4 + i] = 0x3C;    /* middle four rows of the page */
    }

    oled_set_pos(page, 0);
    oled_data(buf, width + 8);
}

void oled_bus_name(char *out, unsigned len)
{
    unsigned scl_no = 0, sda_no = 0;
    unsigned i;

    for (i = 0; i < 16; i++) {
        if (s_scl == (uint16_t)(1U << i)) {
            scl_no = i;
        }
        if (s_sda == (uint16_t)(1U << i)) {
            sda_no = i;
        }
    }
    snprintf(out, len, "PB%u/PB%u", scl_no, sda_no);
}
