#include "w25q64.h"

#define CMD_WRITE_ENABLE    0x06
#define CMD_READ_STATUS1    0x05
#define CMD_READ_DATA       0x03
#define CMD_PAGE_PROGRAM    0x02
#define CMD_SECTOR_ERASE    0x20
#define CMD_JEDEC_ID        0x9F

static void cs(int low)
{
    HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, low ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void sck(int high)
{
    HAL_GPIO_WritePin(W25Q_SCK_PORT, W25Q_SCK_PIN, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void mosi(int high)
{
    HAL_GPIO_WritePin(W25Q_MOSI_PORT, W25Q_MOSI_PIN, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static int miso(void)
{
    return HAL_GPIO_ReadPin(W25Q_MISO_PORT, W25Q_MISO_PIN) == GPIO_PIN_SET;
}

static uint8_t spi_xfer(uint8_t out)
{
    uint8_t in = 0;
    int i;

    for (i = 7; i >= 0; i--) {
        mosi((out >> i) & 1);
        sck(1);
        in = (uint8_t)((in << 1) | (miso() ? 1 : 0));
        sck(0);
    }
    return in;
}

static void write_enable(void)
{
    cs(1);
    (void)spi_xfer(CMD_WRITE_ENABLE);
    cs(0);
}

static void wait_busy(void)
{
    uint32_t guard = 0;

    cs(1);
    (void)spi_xfer(CMD_READ_STATUS1);
    while ((spi_xfer(0xFF) & 0x01) && ++guard < 2000000UL) {
    }
    cs(0);
}

static void gpio_setup(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();   /* 释放 PA15/PB3/PB4，保留 SWD */

    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    g.Pin = W25Q_CS_PIN;
    HAL_GPIO_Init(W25Q_CS_PORT, &g);
    g.Pin = W25Q_SCK_PIN | W25Q_MOSI_PIN;
    HAL_GPIO_Init(W25Q_SCK_PORT, &g);

    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    g.Pin = W25Q_MISO_PIN;
    HAL_GPIO_Init(W25Q_MISO_PORT, &g);

    cs(0);
    sck(0);
}

uint32_t w25q64_read_id(void)
{
    uint32_t id = 0;
    int i;

    cs(1);
    (void)spi_xfer(CMD_JEDEC_ID);
    for (i = 0; i < 3; i++) {
        id = (id << 8) | spi_xfer(0xFF);
    }
    cs(0);
    return id;
}

int w25q64_init(void)
{
    uint32_t id;

    gpio_setup();
    HAL_Delay(5);
    id = w25q64_read_id();
    return (id == 0xEF4017UL) ? 1 : 0;
}

void w25q64_read(uint32_t addr, void *dst, uint32_t len)
{
    uint8_t *p = (uint8_t *)dst;
    uint32_t i;

    cs(1);
    (void)spi_xfer(CMD_READ_DATA);
    (void)spi_xfer((uint8_t)(addr >> 16));
    (void)spi_xfer((uint8_t)(addr >> 8));
    (void)spi_xfer((uint8_t)addr);
    for (i = 0; i < len; i++) {
        p[i] = spi_xfer(0xFF);
    }
    cs(0);
}

int w25q64_page_program(uint32_t addr, const void *src, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)src;
    uint32_t i;

    if (len == 0 || len > 256) return 0;
    write_enable();
    cs(1);
    (void)spi_xfer(CMD_PAGE_PROGRAM);
    (void)spi_xfer((uint8_t)(addr >> 16));
    (void)spi_xfer((uint8_t)(addr >> 8));
    (void)spi_xfer((uint8_t)addr);
    for (i = 0; i < len; i++) {
        (void)spi_xfer(p[i]);
    }
    cs(0);
    wait_busy();
    return 1;
}

int w25q64_sector_erase(uint32_t addr)
{
    write_enable();
    cs(1);
    (void)spi_xfer(CMD_SECTOR_ERASE);
    (void)spi_xfer((uint8_t)(addr >> 16));
    (void)spi_xfer((uint8_t)(addr >> 8));
    (void)spi_xfer((uint8_t)addr);
    cs(0);
    wait_busy();
    return 1;
}