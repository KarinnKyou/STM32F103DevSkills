#ifndef IIC_PROBE_H
#define IIC_PROBE_H

#include "stm32f1xx.h"

/* One candidate pin pair for a software-I2C bus. */
typedef struct {
    const char   *name;   /* label for the report, e.g. "OLED socket B6/B7" */
    GPIO_TypeDef *port;   /* GPIOB on the F103C8T6 boards */
    uint16_t      scl;    /* GPIO_PIN_n */
    uint16_t      sda;    /* GPIO_PIN_n */
    uint8_t       shift;  /* bit position of sda, for IDR reads */
} iic_pair_t;

typedef struct {
    int         found;    /* 1 = something ACKed */
    const char *name;     /* which pair answered */
    uint8_t     addr7;    /* address that answered */
} iic_probe_result_t;

/* Probe every pair for one 7-bit address; returns the first that ACKs.
 * Prints a report over the debug UART if it is already initialised. */
iic_probe_result_t iic_probe_pairs(const iic_pair_t *pairs, int count, uint8_t addr7);

/* This board's two known buses, in the order worth trying. */
extern const iic_pair_t iic_pairs_f103c8_example[];
extern const int        iic_pairs_f103c8_example_count;

#endif
