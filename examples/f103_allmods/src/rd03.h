/* Rd-03 radar on USART3 (PB10 = MCU TX, PB11 = MCU RX), 115200 8N1.
 *
 * The module prints line-based ASCII, one event as two lines:
 *     ON\r\n
 *     Range 35\r\n
 * and only sends Range again when the distance changes, so the display must
 * remember the last value instead of expecting it on every frame.
 */
#ifndef __RD03_H
#define __RD03_H

#include "stm32f1xx.h"

extern UART_HandleTypeDef huart3;

/* -1 = unknown yet, 0 = off, 1 = detecting */
extern volatile int      g_radar_present;
/* last distance in cm, -1 when unknown */
extern volatile int      g_radar_cm;
extern volatile uint32_t g_radar_frames;

void rd03_init(void);

#endif
