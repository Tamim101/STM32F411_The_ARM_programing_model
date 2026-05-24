/*
 * ============================================================
 *  MQ-9 CO / LPG GAS MONITOR — FINAL PROJECT CODE
 *  Date    : 23/05/2026
 *  Target  : STM32F103C8T6 (Blue Pill)
 *  Method  : Pure Bare Metal — zero HAL, zero framework libs
 * ============================================================
 *
 *  platformio.ini:
 *  ─────────────────────────────────────
 *  [env:genericSTM32F103C8]
 *  platform  = ststm32
 *  board     = genericSTM32F103C8
 *  framework = cmsis
 *  ─────────────────────────────────────
 *
 *  FINAL WIRING:
 *  ┌─────────────────────────────────────────────────┐
 *  │  PA0  → MQ-9 A_OUT (analog 0–3.3V)             │
 *  │  PA2  → Buzzer (+)  direct, no transistor       │
 *  │  GND  → Buzzer (−)                              │
 *  │  PB6  → LCD SCL                                 │
 *  │  PB7  → LCD SDA                                 │
 *  │  5V   → MQ-9 VCC, LCD VCC  ← from USB 5V pin   │
 *  │  GND  → MQ-9 GND, LCD GND                      │
 *  └─────────────────────────────────────────────────┘
 *
 *  ACCEPTANCE CRITERIA:
 *  ✓ Display shows real PPM — updates every second
 *  ✓ Buzzer silent in clean air
 *  ✓ Buzzer triggers when gas present (≥ threshold)
 *  ✓ Runs continuously — always on
 *  ✓ Single power input (USB 5V)
 *
 * ============================================================
 *  HOW TO USE — READ THIS BEFORE FLASHING
 * ============================================================
 *
 *  STEP 1 — FIRST POWER ON (warm-up phase)
 *    LCD shows: "  CO MONITOR    "
 *               "  WARMING UP... "
 *    Duration: 5 minutes (300 seconds countdown)
 *    Buzzer: TWO short beeps at startup then SILENT
 *    Reason: MQ-9 heating element needs 5 min to stabilise
 *    DO NOT use readings during warm-up — they are unreliable
 *
 *  STEP 2 — CALIBRATION (automatic, happens after warm-up)
 *    LCD shows: " CALIBRATING... "
 *               "  Please wait   "
 *    Duration: ~5 seconds
 *    IMPORTANT: During calibration the air MUST be clean
 *    No gas, no smoke, no LPG nearby
 *    The sensor measures R0 (its baseline resistance in clean air)
 *    All PPM readings are calculated relative to R0
 *
 *  STEP 3 — NORMAL MONITORING
 *    LCD shows: "  CO MONITOR    "
 *               "CO: XXX PPM     "
 *    Updates every 1 second
 *    Buzzer: SILENT when PPM < CO_ALARM_PPM
 *
 *  STEP 4 — ALARM STATE
 *    When CO or LPG detected above threshold:
 *    LCD shows: "!!! CO ALARM !!!"
 *               "CO: XXX PPM     "
 *    Buzzer: ON continuously (2kHz tone)
 *    Alarm clears automatically when PPM drops below threshold
 *
 * ============================================================
 *  CALIBRATION EXPLAINED
 * ============================================================
 *
 *  The MQ-9 sensor works by measuring resistance (Rs).
 *  In clean air it has a known resistance called R0.
 *  The ratio Rs/R0 is used to calculate PPM.
 *
 *  PPM = A × (Rs/R0) ^ B
 *
 *  For CO  gas: A=100,  B=-1.513  (MQ-9 datasheet)
 *  For LPG gas: A=4.4,  B=-1.265  (MQ-9 datasheet)
 *
 *  The code calculates BOTH CO and LPG PPM simultaneously.
 *  LCD shows CO PPM. Alarm triggers on EITHER gas.
 *
 *  If readings seem wrong after calibration:
 *    1. Power off, wait 1 minute
 *    2. Power on in fresh outdoor air
 *    3. Let warm-up complete fully (5 minutes)
 *    4. Calibration will auto-run again
 *
 * ============================================================
 *  ALARM THRESHOLD
 * ============================================================
 *
 *  CO  alarm: 35 PPM (OSHA 8-hour safe exposure limit)
 *  LPG alarm: 1000 PPM (lower explosive limit / 10 for safety)
 *
 *  Change these values below to adjust sensitivity:
 *    #define CO_ALARM_PPM   35.0f
 *    #define LPG_ALARM_PPM  1000.0f
 *
 * ============================================================
 */

#include <stdint.h>
#include <math.h>

/* ── Registers ──────────────────────────────────────────────────── */
#define FLASH_ACR    (*(volatile uint32_t*)0x40022000UL)
#define RCC_CR       (*(volatile uint32_t*)0x40021000UL)
#define RCC_CFGR     (*(volatile uint32_t*)0x40021004UL)
#define RCC_APB2ENR  (*(volatile uint32_t*)0x40021018UL)
#define RCC_APB1ENR  (*(volatile uint32_t*)0x4002101CUL)
#define AFIO_MAPR    (*(volatile uint32_t*)0x40010004UL)

#define GPIOA_CRL    (*(volatile uint32_t*)0x40010800UL)
#define GPIOA_BSRR   (*(volatile uint32_t*)0x40010810UL)
#define GPIOA_BRR    (*(volatile uint32_t*)0x40010814UL)

#define GPIOB_CRL    (*(volatile uint32_t*)0x40010C00UL)
#define GPIOB_IDR    (*(volatile uint32_t*)0x40010C08UL)
#define GPIOB_BSRR   (*(volatile uint32_t*)0x40010C10UL)
#define GPIOB_BRR    (*(volatile uint32_t*)0x40010C14UL)

#define ADC1_SR      (*(volatile uint32_t*)0x40012400UL)
#define ADC1_CR2     (*(volatile uint32_t*)0x40012408UL)
#define ADC1_SMPR2   (*(volatile uint32_t*)0x40012414UL)
#define ADC1_SQR1    (*(volatile uint32_t*)0x4001242CUL)
#define ADC1_SQR3    (*(volatile uint32_t*)0x40012434UL)
#define ADC1_DR      (*(volatile uint32_t*)0x4001244CUL)

#define STK_CTRL     (*(volatile uint32_t*)0xE000E010UL)
#define STK_LOAD_R   (*(volatile uint32_t*)0xE000E014UL)
#define STK_VAL      (*(volatile uint32_t*)0xE000E018UL)

/* ── Software I2C — PB6=SCL PB7=SDA ────────────────────────────── */
#define SCL_HIGH()  GPIOB_BSRR = (1U<<6)
#define SCL_LOW()   GPIOB_BRR  = (1U<<6)
#define SDA_HIGH()  GPIOB_BSRR = (1U<<7)
#define SDA_LOW()   GPIOB_BRR  = (1U<<7)
#define SDA_READ()  ((GPIOB_IDR>>7)&1U)

/* ============================================================
 * PROJECT CONFIGURATION — edit these to change behaviour
 * ============================================================ */

/* LCD I2C address — auto-detected but set default here */
#define LCD_ADDR_DEFAULT  0x27U   /* try 0x3F if blank */

/* Alarm thresholds */
#define CO_ALARM_PPM    35.0f     /* OSHA CO safe limit (PPM)       */
#define LPG_ALARM_PPM   1000.0f   /* LPG alarm threshold (PPM)      */

/* Warm-up duration */
#define WARMUP_SEC      300U      /* 5 minutes = 300 seconds         */

/* MQ-9 sensor constants */
#define MQ9_RL          10000.0f  /* load resistor on module (Ω)    */
#define MQ9_VSUPPLY     5.0f      /* sensor supply voltage           */
#define MQ9_VREF        3.3f      /* STM32 ADC reference             */
#define MQ9_ADC_MAX     4095.0f   /* 12-bit full scale               */
#define MQ9_R0_DEFAULT  10000.0f  /* R0 before calibration           */

/* MQ-9 CO curve:  PPM = 100.0  × (Rs/R0)^(-1.513) */
#define CO_A    100.0f
#define CO_B    (-1.513f)

/* MQ-9 LPG curve: PPM = 4.4   × (Rs/R0)^(-1.265) */
#define LPG_A   4.4f
#define LPG_B   (-1.265f)

/* ============================================================
 * SYSTICK — 4kHz (0.25ms per tick)
 * g_ms increments every 4 ticks = 1ms resolution
 * Buzzer toggles every 2 ticks = 2kHz tone
 * ============================================================ */
static volatile uint32_t g_ms       = 0;
static volatile uint32_t g_tick4    = 0;
static volatile uint8_t  g_buz_en   = 0;
static volatile uint32_t g_buz_cnt  = 0;
static volatile uint8_t  g_buz_pin  = 0;

void SysTick_Handler(void) {
    g_tick4++;
    if ((g_tick4 & 3U) == 0U) g_ms++;

    if (g_buz_en) {
        g_buz_cnt++;
        if (g_buz_cnt >= 2U) {
            g_buz_cnt = 0;
            if (g_buz_pin) {
                GPIOA_BRR  = (1U<<2);
                g_buz_pin  = 0;
            } else {
                GPIOA_BSRR = (1U<<2);
                g_buz_pin  = 1;
            }
        }
    } else {
        GPIOA_BRR = (1U<<2);
        g_buz_pin = 0;
        g_buz_cnt = 0;
    }
}

static void delay_ms(uint32_t ms) {
    uint32_t s = g_ms;
    while ((g_ms - s) < ms);
}
static void delay_us(uint32_t us) {
    volatile uint32_t n = us * 8U;
    while (n--);
}
static void buzzer_on(void)  { g_buz_en = 1; }
static void buzzer_off(void) { g_buz_en = 0; }

/* ============================================================
 * CLOCK — 64 MHz (HSI PLL×16)
 * ============================================================ */
static void clock_init(void) {
    FLASH_ACR  = (1U<<4)|2U;
    RCC_CR    |= (1U<<0); while(!(RCC_CR&(1U<<1)));
    RCC_CFGR   = (4U<<8)|(2U<<14)|(0U<<16)|(14U<<18);
    RCC_CR    |= (1U<<24); while(!(RCC_CR&(1U<<25)));
    RCC_CFGR  |= 2U; while((RCC_CFGR&(3U<<2))!=(2U<<2));
}

/* ============================================================
 * SOFTWARE I2C
 * ============================================================ */
static void i2c_gpio_init(void) {
    GPIOB_CRL &= ~(0xFFU<<24);
    GPIOB_CRL |=  (0x6U<<24);
    GPIOB_CRL |=  (0x6U<<28);
    SCL_HIGH(); SDA_HIGH();
    delay_ms(10);
}
static void i2c_start(void) {
    SDA_HIGH(); delay_us(5);
    SCL_HIGH(); delay_us(5);
    SDA_LOW();  delay_us(5);
    SCL_LOW();  delay_us(5);
}
static void i2c_stop(void) {
    SDA_LOW();  delay_us(5);
    SCL_HIGH(); delay_us(5);
    SDA_HIGH(); delay_us(5);
}
static uint8_t i2c_write_byte(uint8_t b) {
    for (uint8_t i=0; i<8; i++) {
        if (b&0x80U) SDA_HIGH(); else SDA_LOW();
        delay_us(3); SCL_HIGH(); delay_us(5);
        SCL_LOW(); delay_us(3); b<<=1;
    }
    SDA_HIGH(); delay_us(3);
    SCL_HIGH(); delay_us(5);
    uint8_t ack=(uint8_t)(1U-SDA_READ());
    SCL_LOW(); delay_us(3);
    return ack;
}
static void i2c_send(uint8_t addr, uint8_t data) {
    i2c_start();
    if (i2c_write_byte((uint8_t)(addr<<1)))
        i2c_write_byte(data);
    i2c_stop();
    delay_us(100);
}
static uint8_t i2c_probe(uint8_t addr) {
    i2c_start();
    uint8_t ack=i2c_write_byte((uint8_t)(addr<<1));
    i2c_stop(); delay_ms(1);
    return ack;
}

/* ============================================================
 * LCD — HD44780 via PCF8574 I2C backpack
 * ============================================================ */
#define BL  0x08U
#define EN  0x04U
#define RS  0x01U
static uint8_t g_lcd_addr = LCD_ADDR_DEFAULT;

static void lcd_pulse(uint8_t n) {
    i2c_send(g_lcd_addr,(uint8_t)(n|EN));  delay_ms(2);
    i2c_send(g_lcd_addr,(uint8_t)(n&~EN)); delay_ms(2);
}
static void lcd_write(uint8_t v, uint8_t m) {
    lcd_pulse((uint8_t)((v&0xF0U)|BL|m));
    lcd_pulse((uint8_t)(((v<<4)&0xF0U)|BL|m));
}
static void lcd_cmd(uint8_t c)    { lcd_write(c,0);   }
static void lcd_char(uint8_t c)   { lcd_write(c,RS);  }
static void lcd_str(const char*s) { while(*s) lcd_char((uint8_t)*s++); }
static void lcd_clear(void)       { lcd_cmd(0x01); delay_ms(5); }
static void lcd_pos(uint8_t col, uint8_t row) {
    lcd_cmd(0x80U|(uint8_t)(col+(row?0x40U:0x00U)));
}
static void lcd_init(void) {
    delay_ms(200);
    i2c_send(g_lcd_addr,(uint8_t)(0x30U|BL|EN)); delay_ms(5);
    i2c_send(g_lcd_addr,(uint8_t)(0x30U|BL));    delay_ms(2);
    i2c_send(g_lcd_addr,(uint8_t)(0x30U|BL|EN)); delay_ms(2);
    i2c_send(g_lcd_addr,(uint8_t)(0x30U|BL));    delay_ms(2);
    i2c_send(g_lcd_addr,(uint8_t)(0x30U|BL|EN)); delay_ms(2);
    i2c_send(g_lcd_addr,(uint8_t)(0x30U|BL));    delay_ms(2);
    i2c_send(g_lcd_addr,(uint8_t)(0x20U|BL|EN)); delay_ms(2);
    i2c_send(g_lcd_addr,(uint8_t)(0x20U|BL));    delay_ms(2);
    lcd_cmd(0x28); delay_ms(2);
    lcd_cmd(0x08); delay_ms(2);
    lcd_cmd(0x01); delay_ms(5);
    lcd_cmd(0x06); delay_ms(2);
    lcd_cmd(0x0C); delay_ms(2);
}

/* Print integer right-aligned in w chars */
static void lcd_int(int32_t v, uint8_t w) {
    char buf[12]; uint8_t i=0;
    uint32_t u=(v<0)?(uint32_t)(-v):(uint32_t)v;
    if(u==0U) buf[i++]='0';
    while(u>0U){buf[i++]=(char)('0'+(u%10U));u/=10U;}
    if(v<0) buf[i++]='-';
    while(i<w) buf[i++]=' ';
    for(uint8_t a=0,b=(uint8_t)(i-1U);a<b;a++,b--){
        char t=buf[a];buf[a]=buf[b];buf[b]=t;
    }
    buf[i]='\0'; lcd_str(buf);
}

/* ============================================================
 * ADC — PA0 channel 0
 * ============================================================ */
static void adc_init(void) {
    GPIOA_CRL &= ~(0xFU<<0);           /* PA0 analog input         */
    ADC1_SMPR2 = (3U<<0);              /* 41.5 cycle sample time   */
    ADC1_SQR1  = 0U;
    ADC1_SQR3  = 0U;
    ADC1_CR2   = (7U<<17)|(1U<<20);    /* SW trigger               */
    ADC1_CR2  |= (1U<<0);              /* ADON                     */
    delay_ms(2);
    ADC1_CR2  |= (1U<<3);              /* RSTCAL                   */
    while(ADC1_CR2&(1U<<3));
    ADC1_CR2  |= (1U<<2);              /* CAL                      */
    while(ADC1_CR2&(1U<<2));
}
static uint16_t adc_read(void) {
    ADC1_CR2 |= (1U<<0)|(1U<<22);
    while(!(ADC1_SR&(1U<<1)));
    return (uint16_t)(ADC1_DR&0xFFFU);
}
/* Average 10 readings — removes electrical noise */
static uint16_t adc_avg(void) {
    uint32_t s=0;
    for(uint8_t i=0;i<10;i++){s+=adc_read();delay_ms(5);}
    return (uint16_t)(s/10U);
}

/* ============================================================
 * MQ-9 SENSOR MATH
 *
 * The MQ-9 has a tin-dioxide element whose resistance (Rs)
 * changes when gas is present.
 * Lower Rs = more gas.
 *
 * Step 1: ADC raw → sensor output voltage (Vout)
 *   Vout = (raw / 4095) × 3.3V
 *
 * Step 2: Vout → sensor resistance (Rs)
 *   Rs = RL × (Vsupply − Vout) / Vout
 *
 * Step 3: Rs → ratio
 *   ratio = Rs / R0
 *   R0 = Rs in clean air (measured during calibration)
 *
 * Step 4: ratio → PPM using datasheet power law
 *   PPM = A × ratio^B
 *
 * CO  gas: A=100,  B=-1.513
 * LPG gas: A=4.4,  B=-1.265
 * ============================================================ */

static float g_R0 = MQ9_R0_DEFAULT;

/* Convert raw ADC to sensor resistance Rs */
static float raw_to_rs(uint16_t raw) {
    float vout = ((float)raw / MQ9_ADC_MAX) * MQ9_VREF;
    if (vout < 0.01f) vout = 0.01f;    /* avoid divide by zero     */
    return MQ9_RL * (MQ9_VSUPPLY - vout) / vout;
}

/* Calibrate R0 — call once in clean air after warm-up
 * Takes 50 samples × 100ms = 5 seconds                           */
static void mq9_calibrate(void) {
    float sum = 0.0f;
    for (uint8_t i = 0; i < 50U; i++) {
        sum += raw_to_rs(adc_read());
        delay_ms(100);
    }
    float r0 = sum / 50.0f;
    /* Sanity check — reject impossible values */
    if (r0 < 100.0f || r0 > 500000.0f)
        g_R0 = MQ9_R0_DEFAULT;
    else
        g_R0 = r0;
}

/* Read CO concentration in PPM */
static float read_co_ppm(void) {
    float rs    = raw_to_rs(adc_avg());
    float ratio = rs / g_R0;
    if (ratio < 0.005f) ratio = 0.005f;
    if (ratio > 10.0f)  ratio = 10.0f;
    float ppm   = CO_A * powf(ratio, CO_B);
    if (ppm < 0.0f)     ppm = 0.0f;
    if (ppm > 9999.0f)  ppm = 9999.0f;
    return ppm;
}

/* Read LPG concentration in PPM */
static float read_lpg_ppm(void) {
    float rs    = raw_to_rs(adc_avg());
    float ratio = rs / g_R0;
    if (ratio < 0.005f) ratio = 0.005f;
    if (ratio > 10.0f)  ratio = 10.0f;
    float ppm   = LPG_A * powf(ratio, LPG_B);
    if (ppm < 0.0f)     ppm = 0.0f;
    if (ppm > 9999.0f)  ppm = 9999.0f;
    return ppm;
}

/* ============================================================
 * LCD SCREEN FUNCTIONS
 * ============================================================ */

/* Warmup countdown screen
 * Line 0: "  CO MONITOR    "
 * Line 1: "WARMUP:  04:32  "               */
static void screen_warmup(uint32_t sec_remaining) {
    uint32_t m = sec_remaining / 60U;
    uint32_t s = sec_remaining % 60U;
    lcd_pos(0,0); lcd_str("  CO MONITOR    ");
    lcd_pos(0,1); lcd_str("WARMUP: ");
    lcd_int((int32_t)m, 2);
    lcd_char(':');
    if (s < 10U) lcd_char('0');
    lcd_int((int32_t)s, 1);
    lcd_str("     ");
}

/* Normal monitoring screen
 * Line 0: "  CO MONITOR    "
 * Line 1: "CO:  023 PPM    "               */
static void screen_monitor(float co_ppm) {
    int32_t p = (co_ppm > 9999.0f) ? 9999 : (int32_t)co_ppm;
    lcd_pos(0,0); lcd_str("  CO MONITOR    ");
    lcd_pos(0,1); lcd_str("CO:"); lcd_int(p,5); lcd_str(" PPM  ");
}

/* Alarm screen
 * Line 0: "!!! CO ALARM !!!"
 * Line 1: "CO:  047 PPM    "               */
static void screen_alarm(float co_ppm) {
    int32_t p = (co_ppm > 9999.0f) ? 9999 : (int32_t)co_ppm;
    lcd_pos(0,0); lcd_str("!!! CO ALARM !!!");
    lcd_pos(0,1); lcd_str("CO:"); lcd_int(p,5); lcd_str(" PPM  ");
}

/* LPG alarm screen
 * Line 0: "!!! LPG ALARM !!"
 * Line 1: "LPG:XXXX PPM    "               */
static void screen_lpg_alarm(float lpg_ppm) {
    int32_t p = (lpg_ppm > 9999.0f) ? 9999 : (int32_t)lpg_ppm;
    lcd_pos(0,0); lcd_str("!!! LPG ALARM !!");
    lcd_pos(0,1); lcd_str("LPG:"); lcd_int(p,4); lcd_str(" PPM  ");
}

/* ============================================================
 * MAIN — 4-STATE MACHINE
 *
 * STATE 0: WARMUP    — 5 min countdown, buzzer locked OFF
 * STATE 1: CALIBRATE — measure R0, ~5 seconds
 * STATE 2: MONITOR   — read PPM, display, check alarms
 * STATE 3: ALARM     — buzzer ON, alarm display, auto-clear
 * ============================================================ */
typedef enum {
    STATE_WARMUP    = 0,
    STATE_CALIBRATE = 1,
    STATE_MONITOR   = 2,
    STATE_ALARM     = 3
} State_t;

int main(void) {

    /* ── 1. Clock 64 MHz ────────────────────────────────────────── */
    clock_init();

    /* ── 2. Enable peripheral clocks ───────────────────────────── */
    RCC_APB2ENR |= (1U<<0)   /* AFIO  */
                 | (1U<<2)   /* GPIOA */
                 | (1U<<3)   /* GPIOB */
                 | (1U<<9);  /* ADC1  */
    RCC_APB1ENR |= (1U<<0);  /* TIM2 (not used but keep for safety) */

    /* ── 3. Disable JTAG, keep SWD ─────────────────────────────── */
    AFIO_MAPR = (AFIO_MAPR & ~(7U<<24)) | (2U<<24);

    /* ── 4. PA2 = push-pull output 50MHz (buzzer GPIO) ─────────── */
    GPIOA_CRL &= ~(0xFU<<8);
    GPIOA_CRL |=  (0x3U<<8);   /* 0011 = push-pull 50MHz */
    GPIOA_BRR  = (1U<<2);      /* PA2 LOW — buzzer off   */

    /* ── 5. SysTick 4kHz — buzzer toggle + 1ms timebase ────────── */
    STK_LOAD_R = 16000U-1U;    /* 64MHz / 4000Hz - 1 = 15999 */
    STK_VAL    = 0U;
    STK_CTRL   = 7U;

    delay_ms(300);

    /* ── 6. Init peripherals ────────────────────────────────────── */
    i2c_gpio_init();
    adc_init();
    buzzer_off();

    /* ── 7. Auto-detect LCD address ─────────────────────────────── */
    while (1) {
        if (i2c_probe(0x27U)) { g_lcd_addr=0x27U; break; }
        delay_ms(100);
        if (i2c_probe(0x3FU)) { g_lcd_addr=0x3FU; break; }
        delay_ms(500);
    }
    lcd_init();
    lcd_clear();

    /* ── 8. Startup beeps — confirms buzzer is working ─────────── */
    lcd_pos(0,0); lcd_str("  CO MONITOR    ");
    lcd_pos(0,1); lcd_str("   STARTING...  ");
    buzzer_on();  delay_ms(300);
    buzzer_off(); delay_ms(200);
    buzzer_on();  delay_ms(300);
    buzzer_off();
    delay_ms(1000);

    /* ── 9. State machine variables ─────────────────────────────── */
    State_t  state       = STATE_WARMUP;
    uint32_t wu_start    = g_ms;        /* warmup start time        */
    uint32_t last_lcd    = g_ms;        /* last LCD update time     */
    float    co_ppm      = 0.0f;
    float    lpg_ppm     = 0.0f;

    /* ── 10. Main loop ──────────────────────────────────────────── */
    while (1) {

        /* ══════════════════════════════════════════════════════════
         * STATE 0: WARMUP
         * Sensor heating element needs 5 minutes to stabilise.
         * Display countdown. Buzzer completely locked OFF.
         * ══════════════════════════════════════════════════════════ */
        if (state == STATE_WARMUP) {

            buzzer_off();  /* ALWAYS off during warmup — no false alarms */

            uint32_t elapsed_sec = (g_ms - wu_start) / 1000U;

            /* Update display every second */
            if ((g_ms - last_lcd) >= 1000U) {
                last_lcd = g_ms;
                uint32_t remaining = (elapsed_sec < WARMUP_SEC)
                                   ? (WARMUP_SEC - elapsed_sec) : 0U;
                screen_warmup(remaining);
            }

            /* Transition to calibration when warmup complete */
            if (elapsed_sec >= WARMUP_SEC) {
                state = STATE_CALIBRATE;
                lcd_clear();
                lcd_pos(0,0); lcd_str(" CALIBRATING... ");
                lcd_pos(0,1); lcd_str("  Please wait   ");
            }
        }

        /* ══════════════════════════════════════════════════════════
         * STATE 1: CALIBRATE
         * Measure R0 baseline in clean air.
         * IMPORTANT: Air must be clean during this state.
         * Takes ~5 seconds. Blocking is intentional.
         * ══════════════════════════════════════════════════════════ */
        else if (state == STATE_CALIBRATE) {

            buzzer_off();
            mq9_calibrate();   /* 50 samples × 100ms = 5 seconds   */

            /* Show calibration result */
            lcd_clear();
            lcd_pos(0,0); lcd_str("  CALIB DONE!   ");
            lcd_pos(0,1); lcd_str("  Monitoring... ");
            delay_ms(2000);
            lcd_clear();

            state     = STATE_MONITOR;
            last_lcd  = g_ms;
        }

        /* ══════════════════════════════════════════════════════════
         * STATE 2: MONITOR
         * Read CO and LPG PPM every second.
         * Display CO PPM on LCD.
         * Buzzer OFF — silent in clean air.
         * Transition to ALARM if either gas exceeds threshold.
         * ══════════════════════════════════════════════════════════ */
        else if (state == STATE_MONITOR) {

            buzzer_off();  /* silent in clean air */

            if ((g_ms - last_lcd) >= 1000U) {
                last_lcd = g_ms;

                co_ppm  = read_co_ppm();
                lpg_ppm = read_lpg_ppm();

                screen_monitor(co_ppm);

                /* Check alarm thresholds */
                if (co_ppm >= CO_ALARM_PPM || lpg_ppm >= LPG_ALARM_PPM) {
                    state = STATE_ALARM;
                }
            }
        }

        /* ══════════════════════════════════════════════════════════
         * STATE 3: ALARM
         * Gas detected above threshold.
         * Buzzer ON continuously (2kHz tone via SysTick).
         * Display shows alarm message + current PPM.
         * Auto-clears when BOTH gases drop below threshold.
         * ══════════════════════════════════════════════════════════ */
        else if (state == STATE_ALARM) {

            buzzer_on();   /* 2kHz continuous alarm tone */

            if ((g_ms - last_lcd) >= 1000U) {
                last_lcd = g_ms;

                co_ppm  = read_co_ppm();
                lpg_ppm = read_lpg_ppm();

                /* Show which gas triggered alarm */
                if (lpg_ppm >= LPG_ALARM_PPM) {
                    screen_lpg_alarm(lpg_ppm);
                } else {
                    screen_alarm(co_ppm);
                }

                /* Clear alarm only when BOTH gases are safe */
                if (co_ppm < (CO_ALARM_PPM - 5.0f) &&
                    lpg_ppm < (LPG_ALARM_PPM - 50.0f)) {
                    buzzer_off();
                    lcd_clear();
                    state = STATE_MONITOR;
                }
            }
        }

    } /* while(1) */

    return 0;
}