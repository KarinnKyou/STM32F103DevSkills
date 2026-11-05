/* STM32F103C8T6 + Wildfire 小智 expansion board: drive the five modules
 * that are plugged in at once.
 *
 *   OLED 0.96" I2C   PB10/PB11 (or PB6/PB7 -- probed at boot)
 *   Photoresistor    PA5 (ADC1_IN5) + PA12 (DO)
 *   Active buzzer    PB12
 *   EC11 encoder     PA6/PA7 (software quadrature, 1 kHz) + PA8 button
 *   SG90 servo       PB0 (TIM3_CH3, 50 Hz)
 *
 * Behaviour: the encoder knob moves the servo (3 deg per detent), the button
 * beeps, the OLED shows light level / encoder / servo state, and the same
 * status goes out on USART1 at 115200 for verification from the PC.
 */
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "usart.h"
#include "oled.h"
#include "rd03.h"
#include "w25q64.h"
#include "dht11/bsp_dht11.h"

/* EC11 calibration (measured on this board: 10 detents -> +10 edges, so the
 * encoder gives one quadrature edge per detent), and the servo travel per
 * detent. */
#define ENC_EDGES_PER_DETENT   1
#define ENC_DEG_PER_DETENT     6

/* ---- shared with the ISR ---- */
volatile int32_t  g_enc_count;
volatile uint8_t  g_btn_state;
volatile uint32_t g_btn_events;

/* ---- driver handles ---- */
static TIM_HandleTypeDef htim2;          /* 1 kHz timebase      */
static TIM_HandleTypeDef htim3;          /* servo PWM           */
static ADC_HandleTypeDef hadc1;

/* ---- application state ---- */
static oled_bus_t g_bus;
static uint8_t    g_oled_addr = OLED_ADDR_DEFAULT;
static int        g_oled_ok;
static uint8_t    g_beep_on;
static int16_t    g_servo_angle = 90;     /* 0..180 deg, source of truth */
static int        g_temp = -1;
static int        g_humi = -1;
static uint16_t   g_adc_min = 4095;
static uint16_t   g_adc_max;

/* Quadrature decode: index = (previous_state << 2) | current_state,
 * state = (A << 1) | B.  Every legal edge moves the count by one.
 * The sign is negated in app_tim2_tick() so that a clockwise turn raises
 * the count on this board's A/B wiring. */
static const int8_t quad_table[16] = {
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
};

void app_tim2_tick(void)
{
    static uint8_t prev_state;
    static uint8_t btn_cnt;
    uint8_t a, b, state, raw;

    a = (HAL_GPIO_ReadPin(ENC_PORT, ENC_A_PIN) == GPIO_PIN_SET) ? 1U : 0U;
    b = (HAL_GPIO_ReadPin(ENC_PORT, ENC_B_PIN) == GPIO_PIN_SET) ? 1U : 0U;
    state = (uint8_t)((a << 1) | b);

    g_enc_count -= quad_table[((prev_state << 2) | state) & 0x0F];
    prev_state = state;

    /* PA8 is pulled up; assume the module shorts it to GND when pressed. */
    raw = (HAL_GPIO_ReadPin(ENC_PORT, ENC_SW_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
    if (raw != g_btn_state) {
        if (++btn_cnt >= 12U) {              /* 12 ms debounce */
            g_btn_state = raw;
            btn_cnt = 0;
            if (raw) {
                g_btn_events++;
            }
        }
    } else {
        btn_cnt = 0;
    }
}

/* ======================= clock ======================= */
static void system_clock_config(void)
{
    RCC_ClkInitTypeDef clk = {0};
    RCC_OscInitTypeDef osc = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;                 /* 8 MHz * 9 = 72 MHz */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        while (1) {
        }
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;            /* 36 MHz, timers x2 */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        while (1) {
        }
    }
}

/* ======================= GPIO ======================= */
static void gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* on-board LEDs: 3V3 -> LED -> 1k -> pin, so low = lit */
    gpio.Pin = LED_R_PIN | LED_G_PIN | LED_B_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);
    HAL_GPIO_WritePin(LED_PORT, LED_R_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_PORT, LED_G_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_PORT, LED_B_PIN, GPIO_PIN_SET);

    /* active buzzer: high level = sound */
    gpio.Pin = BUZZ_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZ_PORT, &gpio);
    HAL_GPIO_WritePin(BUZZ_PORT, BUZZ_PIN, GPIO_PIN_RESET);

    /* encoder A/B/SW as inputs with pull-ups (active low) */
    gpio.Pin = ENC_A_PIN | ENC_B_PIN | ENC_SW_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(ENC_PORT, &gpio);

    /* photoresistor digital output */
    gpio.Pin = PHOTO_DO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(PHOTO_PORT, &gpio);

    /* potentiometer: analog threshold input on PA0 */
    gpio.Pin = POT_PIN;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(POT_PORT, &gpio);

    /* thermistor analog input (ADC1_IN4) and its digital output */
    gpio.Pin = NTC_AO_PIN;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(NTC_PORT, &gpio);

    gpio.Pin = NTC_DO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(NTC_PORT, &gpio);

    /* servo PWM pin: alternate function push-pull, driven by TIM3_CH3 */
    gpio.Pin = SERVO_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SERVO_PORT, &gpio);
}

static void led_write(uint16_t pin, int on){
    HAL_GPIO_WritePin(LED_PORT, pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/* ======================= buzzer ======================= */
static void buzzer_set(int on)
{
    g_beep_on = on ? 1U : 0U;
    HAL_GPIO_WritePin(BUZZ_PORT, BUZZ_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    led_write(LED_R_PIN, on);
}

static void delay_us_loop(uint32_t us)
{
    for (volatile uint32_t i = 0; i < us * 18U; i++) {
    }
}

/* Square wave, for a passive buzzer (which stays silent on a DC high).
 * An active buzzer also sounds, just with a rough tone. */
static void buzzer_tone(uint32_t ms, uint32_t half_us)
{
    uint32_t t0 = HAL_GetTick();

    while ((HAL_GetTick() - t0) < ms) {
        HAL_GPIO_WritePin(BUZZ_PORT, BUZZ_PIN, GPIO_PIN_SET);
        delay_us_loop(half_us);
        HAL_GPIO_WritePin(BUZZ_PORT, BUZZ_PIN, GPIO_PIN_RESET);
        delay_us_loop(half_us);
    }
}

/* Audible beep.  The module on IO3 turned out to be a passive buzzer: a DC
 * high is silent, it needs a square wave. */
static void buzzer_beep(uint32_t ms)
{
    g_beep_on = 1U;
    led_write(LED_R_PIN, 1);
    buzzer_tone(ms, 125);          /* 4 kHz */
    g_beep_on = 0U;
    led_write(LED_R_PIN, 0);
}

/* ======================= servo ======================= */
static void servo_init(void)
{
    TIM_ClockConfigTypeDef clock_src = {0};
    TIM_MasterConfigTypeDef master = {0};
    TIM_OC_InitTypeDef oc = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance = SERVO_TIM;
    htim3.Init.Prescaler = 72 - 1;         /* 1 MHz counter: 1 tick = 1 us */
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 20000 - 1;         /* 20 ms -> 50 Hz */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim3);

    clock_src.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim3, &clock_src);
    HAL_TIM_PWM_Init(&htim3);

    master.MasterOutputTrigger = TIM_TRGO_RESET;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim3, &master);

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 1500;                       /* 1.5 ms = mid travel */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &oc, SERVO_TIM_CHANNEL);

    HAL_TIM_PWM_Start(&htim3, SERVO_TIM_CHANNEL);
}

/* pulse in microseconds, 500..2500 */
static void servo_set_us(uint16_t us)
{
    if (us < SERVO_PULSE_MIN_US) {
        us = SERVO_PULSE_MIN_US;
    }
    if (us > SERVO_PULSE_MAX_US) {
        us = SERVO_PULSE_MAX_US;
    }
    __HAL_TIM_SET_COMPARE(&htim3, SERVO_TIM_CHANNEL, us);
}

static uint16_t servo_pulse_us(void)
{
    return (uint16_t)(SERVO_PULSE_MIN_US +
                      (int32_t)g_servo_angle * 2000 / 180);
}

/* Angle is the state; the pulse width is derived, so repeated small steps
 * cannot drift through integer rounding. */
static void servo_set_angle(int32_t ang)
{
    if (ang < 0) {
        ang = 0;
    }
    if (ang > 180) {
        ang = 180;
    }
    g_servo_angle = (int16_t)ang;
    __HAL_TIM_SET_COMPARE(&htim3, SERVO_TIM_CHANNEL, servo_pulse_us());
}

/* ======================= 1 kHz timebase ======================= */
static void tim2_init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 7200 - 1;       /* 72 MHz / 7200 = 10 kHz */
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 10 - 1;            /* 10 kHz / 10 = 1 kHz */
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim2);

    HAL_NVIC_SetPriority(TIM2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);
    HAL_TIM_Base_Start(&htim2);
}

/* ======================= ADC ======================= */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    GPIO_InitTypeDef gpio = {0};

    if (hadc->Instance != ADC1) {
        return;
    }
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = PHOTO_AO_PIN;
    gpio.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(PHOTO_PORT, &gpio);
}

static void adc_init(void)
{
    ADC_ChannelConfTypeDef ch = {0};

    __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);   /* 72 / 6 = 12 MHz (max 14) */

    hadc1.Instance = PHOTO_ADC;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfDiscConversion = 0;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    HAL_ADC_Init(&hadc1);

    ch.Channel = PHOTO_ADC_CHANNEL;
    ch.Rank = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &ch);

    HAL_ADCEx_Calibration_Start(&hadc1);
}

static uint16_t adc_read_ch(uint32_t channel)
{
    ADC_ChannelConfTypeDef ch = {0};
    uint16_t v = 0;

    ch.Channel = channel;
    ch.Rank = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &ch);

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        v = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);
    return v;
}

static uint16_t adc_read(void)
{
    return adc_read_ch(PHOTO_ADC_CHANNEL);
}

/* ======================= OLED discovery ======================= */
static int i2c_scan(uint16_t scl, uint16_t sda, const char *label)
{
    int addr;
    int found = 0;

    printf("  bus %s:", label);
    for (addr = 0x08; addr < 0x78; addr++) {
        if (soft_i2c_probe(GPIOB, scl, sda, (uint8_t)addr)) {
            printf(" 0x%02X", addr << 1);
            found++;
        }
    }
    printf("   (%d device%s)\r\n", found, found == 1 ? "" : "s");
    return found;
}

/* Read a line as an input with a given pull: tells us whether something on
 * the board is holding it high or low while nobody is talking. */
static int pin_idle_level(uint16_t pin, uint32_t pull)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = pull;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_Delay(1);
    return (HAL_GPIO_ReadPin(GPIOB, pin) == GPIO_PIN_SET) ? 1 : 0;
}

static void oled_discover(void)
{
    static const struct {
        uint16_t scl;
        uint16_t sda;
        const char *name;
    } cand[] = {
        { GPIO_PIN_6,  GPIO_PIN_7,  "SCL=PB6  SDA=PB7  (OLED socket)" },
        { GPIO_PIN_7,  GPIO_PIN_6,  "SCL=PB7  SDA=PB6  (OLED socket, swapped)" },
        { GPIO_PIN_10, GPIO_PIN_11, "SCL=PB10 SDA=PB11 (comms 2 / VCC4)" },
        { GPIO_PIN_11, GPIO_PIN_10, "SCL=PB11 SDA=PB10 (comms 2, swapped)" },
    };
    static const uint8_t addrs[] = { 0x3C, 0x3D };
    int n10, n67;
    unsigned c, a;

    printf("I2C idle levels (PB10=SCL, PB11=SDA):\r\n");
    printf("  pull-up: SCL=%d SDA=%d   pull-down: SCL=%d SDA=%d\r\n",
           pin_idle_level(GPIO_PIN_10, GPIO_PULLUP),
           pin_idle_level(GPIO_PIN_11, GPIO_PULLUP),
           pin_idle_level(GPIO_PIN_10, GPIO_PULLDOWN),
           pin_idle_level(GPIO_PIN_11, GPIO_PULLDOWN));

    n67 = i2c_scan(GPIO_PIN_6, GPIO_PIN_7, "PB6/PB7");
    n10 = i2c_scan(GPIO_PIN_10, GPIO_PIN_11, "PB10/PB11");
    if (n10 > 8 || n67 > 8) {
        printf("  WARNING: many addresses ACK -> SDA is probably stuck low"
               " (unpowered/duff module), the ACK is not real\r\n");
    }

    for (c = 0; c < sizeof(cand) / sizeof(cand[0]); c++) {
        for (a = 0; a < sizeof(addrs) / sizeof(addrs[0]); a++) {
            int ack = soft_i2c_probe(GPIOB, cand[c].scl, cand[c].sda, addrs[a]);
            printf("  probe %s addr=0x%02X -> %s\r\n", cand[c].name, addrs[a] * 2,
                   ack ? "ACK" : "no ack");
            if (ack && !g_oled_ok) {
                g_bus.port = GPIOB;
                g_bus.scl = cand[c].scl;
                g_bus.sda = cand[c].sda;
                g_oled_addr = addrs[a];
                g_oled_ok = oled_init(&g_bus, g_oled_addr);
                if (g_oled_ok) {
                    char bus_name[16];
                    oled_bus_name(bus_name, sizeof(bus_name));
                    printf("  -> OLED initialised on %s (addr 0x%02X)\r\n",
                           bus_name, g_oled_addr * 2);

                    /* panel-level test: all pixels on, then back to RAM */
                    printf("  -> panel test: whole screen white for 1 s\r\n");
                    oled_fill(0xFF);
                    oled_all_on(1);
                    HAL_Delay(600);
                    oled_all_on(0);
                    HAL_Delay(300);

                    oled_line(0, "F103C8T6 ALL-MODULES");
                    oled_line(1, "OLED I2C %s", bus_name);
                    return;
                }
                printf("  -> init failed (NACK while writing commands)\r\n");
            }
        }
    }
    printf("  -> no OLED found on any probed bus\r\n");
}



/* ============================================================
 *  桌面环境与安防监测站  f103_station
 *
 *  十个模块：
 *    OLED / 光敏 / 热敏 / EC11 / SG90 / 蜂鸣器
 *    Rd-03 雷达 (USART3, 通讯接口2 前四针)
 *    DHT11     (PB9, 通讯接口2 第六针)
 *    W25Q64    (软 SPI: CS=A15 DO=B3 DI=B4 CLK=B5)
 *    电位器     (PA0)
 * ============================================================ */

#define TEMP_ALARM_C        30
#define HUMI_ALARM_PCT      70
#define DARK_ADC_TH         1200

#define ALARM_TEMP  0x01
#define ALARM_HUMI  0x02
#define ALARM_NEAR  0x04
#define ALARM_DARK  0x08

#define LOG_BASE      0x000000UL
#define LOG_MAX       512
#define LOG_REC_SIZE  8

static int      g_page;
static int      g_muted;
static int      g_light = -1;
static int      g_ntc = -1;
static uint32_t g_alarm_events;
static uint32_t g_last_alarm_ms;
static int      g_flash_ok;
static int      g_log_count;
static int      g_near_th = 80;
static uint32_t g_prev_alarm;

static int map_i(int v, int in_lo, int in_hi, int out_lo, int out_hi)
{
    if (in_hi == in_lo) return out_lo;
    if (v < in_lo) v = in_lo;
    if (v > in_hi) v = in_hi;
    return out_lo + (v - in_lo) * (out_hi - out_lo) / (in_hi - in_lo);
}

/* ---------------- flash log ---------------- */
static void log_read_rec(int i, uint8_t *rec)
{
    w25q64_read(LOG_BASE + (uint32_t)i * LOG_REC_SIZE, rec, LOG_REC_SIZE);
}

static uint32_t rec_time(const uint8_t *rec)
{
    return ((uint32_t)rec[0] << 24) | ((uint32_t)rec[1] << 16)
         | ((uint32_t)rec[2] << 8) | rec[3];
}

static void log_restore(void)
{
    int i;
    uint8_t rec[LOG_REC_SIZE];

    g_log_count = 0;
    if (!g_flash_ok) return;
    for (i = 0; i < LOG_MAX; i++) {
        log_read_rec(i, rec);
        if (rec[0] == 0xFF && rec[4] == 0xFF) break;
        g_log_count++;
    }
    printf("flash log: %d record(s)\r\n", g_log_count);
}

static void log_append(uint32_t t, uint8_t mask)
{
    uint8_t rec[LOG_REC_SIZE] = {0};

    if (!g_flash_ok || g_log_count >= LOG_MAX) return;
    rec[0] = (uint8_t)(t >> 24);
    rec[1] = (uint8_t)(t >> 16);
    rec[2] = (uint8_t)(t >> 8);
    rec[3] = (uint8_t)t;
    rec[4] = mask;
    w25q64_page_program(LOG_BASE + (uint32_t)g_log_count * LOG_REC_SIZE, rec, LOG_REC_SIZE);
    g_log_count++;
}

static void log_clear(void)
{
    if (!g_flash_ok) return;
    w25q64_sector_erase(LOG_BASE);
    g_log_count = 0;
    printf("flash log cleared\r\n");
}

/* ---------------- pages ---------------- */
static void ui_overview(int alarm)
{
    oled_line(0, "STATION  %s", alarm ? "!ALARM!" : "  OK");
    oled_line(1, "T %2dC  H %2d%%", g_temp, g_humi);
    oled_line(2, "LIGHT %4d", g_light);
    oled_bar(3, (uint16_t)map_i(g_light, 0, 4095, 0, 1000));
    oled_line(4, "NTC %4d DARK<%4d", g_ntc, g_near_th);
    oled_line(5, "ALARM# %-4lu MU %d", (unsigned long)g_alarm_events, g_muted);
    oled_line(6, "SERVO %3d deg", g_servo_angle);
    oled_line(7, "PAGE 1/4   ENC=TURN");
}

static void ui_env(void)
{
    oled_line(0, "ENVIRONMENT");
    oled_line(1, "LIGHT %4d", g_light);
    oled_bar(2, (uint16_t)map_i(g_light, 0, 4095, 0, 1000));
    oled_line(3, "NTC   %4d", g_ntc);
    oled_line(4, "T %2dC  H %2d%%", g_temp, g_humi);
    oled_line(5, "DARK< %4d  %s", g_near_th, g_light < g_near_th ? "DARK" : " ok ");
    oled_line(6, "SERVO %3d deg", g_servo_angle);
    oled_line(7, "PAGE 2/4");
}

static void ui_system(void)
{
    oled_line(0, "SYSTEM");
    oled_line(1, "UPTIME %5lu s", (unsigned long)(HAL_GetTick() / 1000U));
    oled_line(2, "TH T>%2dC H>%d%%", TEMP_ALARM_C, HUMI_ALARM_PCT);
    oled_line(3, "LOG %3d/%d %s", g_log_count, LOG_MAX, g_flash_ok ? "ok" : "--");
    oled_line(4, "ALARM# %lu  MU %d", (unsigned long)g_alarm_events, g_muted);
    oled_line(5, "SERVO %4u us", servo_pulse_us());
    oled_line(6, "ENC=TURN BTN=MUTE");
    oled_line(7, "PAGE 3/4");
}

static void ui_log(void)
{
    int i, start;
    uint8_t rec[LOG_REC_SIZE];

    oled_line(0, "LOG %d/%d", g_log_count, LOG_MAX);
    start = g_log_count - 3;
    if (start < 0) start = 0;
    for (i = 0; i < 3; i++) {
        int idx = start + i;
        if (idx >= g_log_count) {
            oled_line((uint8_t)(1 + i * 2), "  --");
            oled_line((uint8_t)(2 + i * 2), "");
            continue;
        }
        log_read_rec(idx, rec);
        oled_line((uint8_t)(1 + i * 2), "#%d t=%lus", idx + 1,
                  (unsigned long)(rec_time(rec) / 1000U));
        oled_line((uint8_t)(2 + i * 2), "    mask 0x%02X", rec[4]);
    }
    oled_line(7, "PAGE 4/4  BTN=CLR");
}
static void ui_draw(int alarm)
{
    if (!g_oled_ok) return;
    switch (g_page) {
    case 1:  ui_env();      break;
    case 2:  ui_system();   break;
    case 3:  ui_log();      break;
    default: ui_overview(alarm); break;
    }
}

/* ---------------- main ---------------- */
int main(void)
{
    uint32_t last_dht = 0, last_slow = 0, last_key = 0;
    uint32_t last_ui = 0, last_log = 0, last_sweep = 0;
    int32_t  last_detents = 0;
    int      sweep_dir = 1;
    int      alarm = 0;

    HAL_Init();
    system_clock_config();
    usart_init();

    printf("\r\n\r\n=== Desk monitor station (10 modules) ===\r\n");
    printf("SYSCLK=%lu Hz\r\n", (unsigned long)HAL_RCC_GetHCLKFreq());

    gpio_init();
    adc_init();
    servo_init();
    tim2_init();

    servo_set_us(1500);
    HAL_Delay(400);
    g_servo_angle = 90;
    buzzer_beep(120);

    oled_discover();
    /* rd03_init();  -- 雷达模块串口无输出，已从项目移除 */
    DHT11_Init();

    g_flash_ok = w25q64_init();
    printf("W25Q64: %s (id=0x%06lX)\r\n", g_flash_ok ? "ok" : "NOT FOUND",
           (unsigned long)w25q64_read_id());
    if (g_flash_ok) log_restore();

    g_page = 0;
    led_write(LED_B_PIN, 1);
    printf("ready: knob=page, btn=mute (page4=clear)\r\n");

    while (1) {
        uint32_t now = HAL_GetTick();
        int32_t  detents, delta;

        if (now - last_dht >= 2000U) {
            DHT11_Data_TypeDef d;
            last_dht = now;
            if (DHT11_Read_TempAndHumidity(&d) == SUCCESS) {
                g_temp = d.temp_int;
                g_humi = d.humi_int;
            }
        }
        if (now - last_slow >= 200U) {
            uint16_t pot;
            last_slow = now;
            g_light = adc_read_ch(PHOTO_ADC_CHANNEL);
            g_ntc   = adc_read_ch(TRACK_ADC_CHANNEL);
            pot     = adc_read_ch(POT_ADC_CHANNEL);
            g_near_th = map_i(pot, 0, 4095, 200, 3800);   /* 电位器 = 光照阈值 */
        }

        detents = g_enc_count;
        delta = detents - last_detents;
        if (delta != 0) {
            last_detents = detents;
            g_page = (int)((g_page + (delta > 0 ? 1 : -1) + 4) % 4);
        }
        if (g_btn_events != last_key) {
            last_key = g_btn_events;
            if (g_page == 3) {
                log_clear();
            } else {
                g_muted = !g_muted;
            }
        }

        alarm = 0;
        if (g_temp >= TEMP_ALARM_C)                    alarm |= ALARM_TEMP;
        if (g_humi >= HUMI_ALARM_PCT)                  alarm |= ALARM_HUMI;
        if (g_light >= 0 && g_light < g_near_th)       alarm |= ALARM_DARK;
        if (g_light >= 0 && g_light < g_near_th)       alarm |= ALARM_DARK;

        if (alarm && alarm != (int)g_prev_alarm) {
            log_append(now, (uint8_t)alarm);
            printf("event: mask=0x%02X t=%lu log=%d\r\n",
                   alarm, (unsigned long)now, g_log_count);
        }
        g_prev_alarm = (uint32_t)alarm;

        if (alarm && !g_muted) {
            if (g_last_alarm_ms == 0 || (now - g_last_alarm_ms) >= 900U) {
                g_last_alarm_ms = now;
                g_alarm_events++;
                buzzer_beep((alarm & ALARM_NEAR) ? 60 : 40);
            }
        } else if (!alarm) {
            g_last_alarm_ms = 0;
        }

        if (g_radar_present == 1 && g_radar_cm > 0) {
            servo_set_angle(map_i(g_radar_cm, 20, 300, 10, 170));
        } else if (now - last_sweep >= 1200U) {
            int a = g_servo_angle + sweep_dir * 20;
            last_sweep = now;
            if (a >= 140) { a = 140; sweep_dir = -1; }
            if (a <= 40)  { a = 40;  sweep_dir = 1; }
            servo_set_angle(a);
        }

        if (now - last_ui >= 100U) {
            last_ui = now;
            led_write(LED_G_PIN, alarm ? ((now / 200U) & 1U) : 0);
            ui_draw(alarm);
        }

        if (now - last_log >= 1000U) {
            last_log = now;
            printf("t=%5lus T=%2dC H=%2d%% light=%4d ntc=%4d "
                   "near=%3dcm page=%d mute=%d alarm=0x%02X log=%d oled=%s\r\n",
                   (unsigned long)(now / 1000U), g_temp, g_humi,
                   g_light, g_ntc, g_near_th,
                   g_page, g_muted, alarm, g_log_count,
                   g_oled_ok ? "ok" : "none");
        }
    }
}
