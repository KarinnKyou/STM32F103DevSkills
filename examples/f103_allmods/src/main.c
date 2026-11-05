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

/* ======================= UI ======================= */
static unsigned pct_from_adc(uint16_t adc)
{
    return (unsigned)((uint32_t)adc * 100U / 4095U);
}

static void ui_update(uint16_t adc, uint16_t ntc, int32_t detents, int do_level)
{
    char bus_name[16] = "none";

    if (!g_oled_ok) {
        return;
    }
    oled_bus_name(bus_name, sizeof(bus_name));
    oled_line(1, "OLED I2C %s", bus_name);
    oled_line(2, "TEMP %2dC  HUMI %2d%%", g_temp, g_humi);
    oled_line(3, "TRACK %4u  %3u%%", ntc, pct_from_adc(ntc));
    oled_line(4, "ENC %+6ld  ANG %3d", (long)detents, g_servo_angle);
    oled_line(5, "SW %d  BEEP %d  DO %d", g_btn_state, g_beep_on, do_level);
    oled_line(6, "SERVO %4u us", servo_pulse_us());
    if (g_radar_present >= 0) {
        oled_line(7, "RADAR %3dcm %s", g_radar_cm, g_radar_present ? "ON" : "--");
    } else {
        oled_line(7, "RADAR -- (no data)");
    }
}

/* ======================= main ======================= */
int main(void)
{
    uint32_t last_ui = 0, last_log = 0, last_btn = 0;
    int32_t  last_detents = 0;
    uint16_t adc = 0;
    uint16_t ntc = 0;
    int      do_level = 1;

    HAL_Init();
    system_clock_config();
    usart_init();
    rd03_init();
    DHT11_Init();

    printf("\r\n\r\n=== STM32F103C8T6 all-module demo ===\r\n");
    printf("SYSCLK=%lu Hz\r\n", (unsigned long)HAL_RCC_GetHCLKFreq());

    gpio_init();
    adc_init();
    servo_init();
    tim2_init();

    printf("servo self-test (PB0, 500us / 1500us / 2500us)\r\n");
    servo_set_us(500);
    HAL_Delay(700);
    servo_set_us(2500);
    HAL_Delay(700);
    servo_set_us(1500);
    HAL_Delay(400);
    g_servo_angle = 90;

    printf("buzzer self-test (PB12): DC high 200 ms, then 4 kHz square 600 ms\r\n");
    buzzer_set(1);
    HAL_Delay(200);
    buzzer_set(0);
    buzzer_tone(600, 125);

    oled_discover();
    rd03_init();      /* 必须在 OLED 探测之后：探测会把 B10/B11 切成 GPIO */
    DHT11_Init();

    printf("EC11 idle lines: A=%d B=%d SW=%d (all high = module at rest or absent)\r\n",
           (HAL_GPIO_ReadPin(ENC_PORT, ENC_A_PIN) == GPIO_PIN_SET) ? 1 : 0,
           (HAL_GPIO_ReadPin(ENC_PORT, ENC_B_PIN) == GPIO_PIN_SET) ? 1 : 0,
           (HAL_GPIO_ReadPin(ENC_PORT, ENC_SW_PIN) == GPIO_PIN_SET) ? 1 : 0);

    led_write(LED_B_PIN, 1);         /* blue LED: firmware alive */

    while (1) {
        uint32_t now = HAL_GetTick();
        int32_t  detents;
        int32_t  delta;

        adc = adc_read();
        ntc = adc_read_ch(NTC_ADC_CHANNEL);

        {
            static uint32_t last_dht;
            if (now - last_dht >= 2000U) {
                DHT11_Data_TypeDef d;
                last_dht = now;
                if (DHT11_Read_TempAndHumidity(&d) == SUCCESS) {
                    g_temp = d.temp_int;
                    g_humi = d.humi_int;
                }
            }
        }
        if (adc < g_adc_min) {
            g_adc_min = adc;
        }
        if (adc > g_adc_max) {
            g_adc_max = adc;
        }
        do_level = (HAL_GPIO_ReadPin(PHOTO_PORT, PHOTO_DO_PIN) == GPIO_PIN_SET) ? 1 : 0;

        /* one detent per quadrature edge -> the servo follows the knob */
        detents = g_enc_count / ENC_EDGES_PER_DETENT;
        delta = detents - last_detents;
        last_detents = detents;
#if SERVO_DEMO_SWEEP
        /* Demo build: sweep 0..180..0 by itself, ignore the knob. */
        {
            static uint32_t last_step;
            static int32_t  dir = 1;

            if (now - last_step >= 1000U) {
                int32_t a = g_servo_angle + dir * 60;

                last_step = now;
                if (a >= 180) {
                    a = 180;
                    dir = -1;
                } else if (a <= 0) {
                    a = 0;
                    dir = 1;
                }
                servo_set_angle(a);
            }
        }
#endif
#if !SERVO_DEMO_SWEEP
        if (delta != 0) {
            servo_set_angle((int32_t)g_servo_angle + delta * ENC_DEG_PER_DETENT);
        }
#endif

        /* button: short beep */
        if (g_btn_events != last_btn) {
            last_btn = g_btn_events;
            buzzer_beep(60);
        }

        if (now - last_ui >= 100U) {
            last_ui = now;
            ui_update(adc, ntc, detents, do_level);
        }
        if (now - last_log >= 500U) {
            int a = (HAL_GPIO_ReadPin(ENC_PORT, ENC_A_PIN) == GPIO_PIN_SET) ? 1 : 0;
            int b = (HAL_GPIO_ReadPin(ENC_PORT, ENC_B_PIN) == GPIO_PIN_SET) ? 1 : 0;
            int sw_raw = (HAL_GPIO_ReadPin(ENC_PORT, ENC_SW_PIN) == GPIO_PIN_SET) ? 1 : 0;

            last_log = now;
            led_write(LED_G_PIN, (now / 500U) & 1U);
            printf("t=%5lus ntc=%4u(%3u%%) enc=%+6ld ab=%d%d sw=%d/raw%d "
                   "ang=%3d servo=%4u us dht=%dC/%d%% radar=%d/%dcm oled=%s\r\n",
                   (unsigned long)(now / 1000U), ntc, (unsigned)pct_from_adc(ntc),
                   (long)detents, a, b, g_btn_state, sw_raw, g_servo_angle,
                   servo_pulse_us(), g_temp, g_humi, g_radar_present, g_radar_cm,
                   g_oled_ok ? "ok" : "none");
        }
    }
}
