/* Board-level pin map and shared state for the F103C8T6 + expansion board
 * demo (OLED, photoresistor, active buzzer, EC11 encoder, SG90 servo).
 *
 * Wiring assumed (Wildfire 小智 expansion board):
 *   OLED        I2C  -> PB10/PB11 (comms socket 2, VCC4 -> 3.3 V), with
 *                       fallback probing of PB6/PB7 (OLED socket)
 *   Photo       IO1  -> PA5  (analog, ADC1_IN5) + PA12 (digital comparator)
 *   Buzzer      IO3  -> PB12 (3.3 V fixed)
 *   EC11        enc  -> PA6/PA7 quadrature, PA8 push button
 *   SG90        servo 1 -> PB0 (TIM3_CH3, 50 Hz)
 *   Debug UART  USART1 -> PA9 (TX) / PA10 (RX), 115200 8N1
 */
#ifndef __APP_H
#define __APP_H

#include "stm32f1xx_hal.h"

/* ---- debug UART ---- */
#define DBG_USART               USART1

/* ---- on-board LEDs (active low) ---- */
#define LED_PORT                GPIOA
#define LED_R_PIN               GPIO_PIN_1
#define LED_G_PIN               GPIO_PIN_2
#define LED_B_PIN               GPIO_PIN_3

/* ---- photoresistor on 通用IO1 ---- */
#define PHOTO_ADC               ADC1
#define PHOTO_ADC_CHANNEL       ADC_CHANNEL_5      /* PA5 */
#define PHOTO_PORT              GPIOA
#define PHOTO_AO_PIN            GPIO_PIN_5
#define PHOTO_DO_PIN            GPIO_PIN_12

/* ---- thermistor on 通用IO2 (VCC1, GND, A11, A4) ---- */
#define NTC_ADC_CHANNEL         ADC_CHANNEL_4      /* PA4 */
#define NTC_PORT                GPIOA
#define NTC_AO_PIN              GPIO_PIN_4
#define NTC_DO_PIN              GPIO_PIN_11

/* ---- tracking sensor on 通用IO2 (VCC1, GND, A11, A4) ---- */
#define TRACK_ADC_CHANNEL       ADC_CHANNEL_4      /* PA4 */
#define TRACK_PORT              GPIOA
#define TRACK_AO_PIN            GPIO_PIN_4
#define TRACK_DO_PIN            GPIO_PIN_11

/* ---- potentiometer for the distance threshold (PA0, ADC1_IN0, dupont wires) ---- */
#define POT_ADC_CHANNEL         ADC_CHANNEL_0
#define POT_PORT                GPIOA
#define POT_PIN                 GPIO_PIN_0

/* ---- active buzzer on 通用IO3 ---- */
#define BUZZ_PORT               GPIOB
#define BUZZ_PIN                GPIO_PIN_12

/* ---- EC11 encoder socket ---- */
#define ENC_PORT                GPIOA
#define ENC_A_PIN               GPIO_PIN_6
#define ENC_B_PIN               GPIO_PIN_7
#define ENC_SW_PIN              GPIO_PIN_8

/* ---- SG90 on servo socket 1 ---- */
#define SERVO_PORT              GPIOB
#define SERVO_PIN               GPIO_PIN_0
#define SERVO_TIM               TIM3
#define SERVO_TIM_CHANNEL       TIM_CHANNEL_3
/* TIM3 runs at 1 MHz with a 20 ms period, so one compare tick = 1 us. */
#define SERVO_PULSE_MIN_US      500U    /* 0.5 ms -> ~0 deg   */
#define SERVO_PULSE_MID_US      1500U   /* 1.5 ms -> ~90 deg  */
#define SERVO_PULSE_MAX_US      2500U   /* 2.5 ms -> ~180 deg */

/* ---- state shared with the 1 kHz timebase ISR ---- */
extern volatile int32_t  g_enc_count;      /* raw quadrature edges       */
extern volatile uint8_t  g_btn_state;      /* 1 = pressed                */
extern volatile uint32_t g_btn_events;     /* number of press edges      */

void app_tim2_tick(void);                  /* called from TIM2_IRQHandler */

#endif /* __APP_H */
