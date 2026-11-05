/* Software-I2C bus probe — find which pin pair a device actually answers on.
 *
 * Why this exists: on the Wildfire F103C8T6 kit the same OLED module works on
 * the core board pin header at SCL=PB10/SDA=PB11 but on the expansion board's
 * OLED socket at SCL=PB6/SDA=P7. Guessing a mapping and "looking at whether the
 * screen lit up" is unreliable; the firmware should discover the bus and say so.
 *
 * Two rules learned the hard way:
 *   1. Calibrate the probe on a known-good bus before trusting it. If SDA is
 *      stuck low, a naive probe reads "ACK" from every address.
 *   2. Report over UART. "Did the screen light up" is not evidence; a printed
 *      line with the bus name and the ACKing address is.
 */
#include "iic_probe.h"
#include <stdio.h>

static void dly(void)
{
    for (volatile int i = 0; i < 120; i++) {
    }
}

static void pin_init(const iic_pair_t *p)
{
    GPIO_InitTypeDef g = {0};

    if (p->port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    if (p->port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    if (p->port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();

    g.Pin = p->scl | p->sda;
    g.Mode = GPIO_MODE_OUTPUT_OD;     /* open drain: the device can hold SDA low */
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(p->port, &g);

    p->port->BSRR = p->scl | p->sda;  /* idle high */
}

static void scl_w(const iic_pair_t *p, int v)
{
    p->port->BSRR = v ? p->scl : (uint32_t)p->scl << 16;
}

static void sda_w(const iic_pair_t *p, int v)
{
    p->port->BSRR = v ? p->sda : (uint32_t)p->sda << 16;
}

static int sda_r(const iic_pair_t *p)
{
    return (p->port->IDR & p->sda) != 0;
}

/* Returns 1 if the device ACKed, 0 if not. */
static int probe_addr(const iic_pair_t *p, uint8_t addr7)
{
    int i, bit, ack;

    /* Bus reset in case a previous run left the device mid-transfer. */
    for (i = 0; i < 9; i++) {
        scl_w(p, 0); dly(); scl_w(p, 1); dly();
    }

    sda_w(p, 1); dly(); scl_w(p, 1); dly();
    sda_w(p, 0); dly();                 /* START */
    scl_w(p, 0); dly();

    for (i = 7; i >= 0; i--) {
        bit = (addr7 >> i) & 1;
        sda_w(p, bit); dly();
        scl_w(p, 1); dly();
        scl_w(p, 0); dly();
    }

    sda_w(p, 1); dly();                 /* release SDA for the ACK bit */
    scl_w(p, 1); dly();
    ack = !sda_r(p);                    /* device pulls low = ACK */
    scl_w(p, 0); dly();

    sda_w(p, 0); dly(); scl_w(p, 1); dly(); sda_w(p, 1); dly();  /* STOP */
    return ack;
}

iic_probe_result_t iic_probe_pairs(const iic_pair_t *pairs, int count, uint8_t addr7)
{
    iic_probe_result_t r = {0, 0, 0};
    int i;

    for (i = 0; i < count; i++) {
        pin_init(&pairs[i]);
        dly();
        if (probe_addr(&pairs[i], addr7)) {
            r.found = 1;
            r.name = pairs[i].name;
            r.addr7 = addr7;
            printf("  bus %-22s 0x%02X  (ACK)\r\n", pairs[i].name, addr7);
            break;
        }
        printf("  bus %-22s 0x%02X  (no device)\r\n", pairs[i].name, addr7);
    }
    if (r.found) {
        printf("  -> found device on %s (addr 0x%02X)\r\n", r.name, addr7);
    } else {
        printf("  -> no device on any probed bus\r\n");
    }
    return r;
}

/* Wildfire F103C8T6: expansion-board OLED socket first, then the core-board
 * pin header mapping from the vendor pin table. */
const iic_pair_t iic_pairs_f103c8_example[] = {
    { "OLED socket PB6/PB7", GPIOB, GPIO_PIN_6,  GPIO_PIN_7,  7  },
    { "header PB10/PB11",    GPIOB, GPIO_PIN_10, GPIO_PIN_11, 11 },
};
const int iic_pairs_f103c8_example_count =
    (int)(sizeof(iic_pairs_f103c8_example) / sizeof(iic_pairs_f103c8_example[0]));
