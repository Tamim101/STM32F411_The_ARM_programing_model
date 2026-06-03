/*
 * ============================================================
 *  MQ-9 CO / LPG GAS MONITOR — SIMPLE PPM EDITION
 *  STM32F103C8T6 (Blue Pill) — Bare Metal, zero HAL
 *
 *  KH158-style alarm behaviour.
 *
 *  ── THE CALIBRATION PROBLEM EXPLAINED ──────────────────
 *
 *  The MQ-9 sensor works like this:
 *
 *    CLEAN AIR  →  sensor resistance is HIGH  →  this value = R0
 *    GAS PRESENT → sensor resistance DROPS    →  this value = Rs
 *    PPM = 100 × (Rs / R0) ^ (-1.513)         [for CO]
 *
 *  R0 is your personal sensor's resistance in clean air.
 *  Every MQ-9 chip is slightly different — you MUST measure
 *  YOUR sensor's R0 in clean air, once, at first boot.
 *
 *  WHY YOUR FIRST READINGS WERE WRONG:
 *  The old code used BASELINE_CO_PPM = 8ppm to guess R0
 *  from roadside air. That guess was inaccurate because:
 *    1. Roadside CO is not a fixed known value.
 *    2. The MQ-9 also responds to humidity and temperature.
 *    3. A bad R0 makes EVERY future PPM reading wrong.
 *
 *  THE FIX:
 *  Calibrate in CLEAN AIR (early morning, open window,
 *  away from traffic and cooking). The sensor measures Rs
 *  in that clean air and stores it as R0. From then on,
 *  PPM is calculated correctly.
 *
 *  WHAT IS CLEAN AIR FOR THIS PURPOSE:
 *    ✓ Open rooftop, early morning
 *    ✓ Indoors, early morning, windows open, no cooking
 *    ✓ Open field away from roads
 *    ✗ Roadside (CO 5–50 ppm — too high)
 *    ✗ Kitchen while cooking
 *    ✗ Garage or basement
 *    ✗ Near a car or generator
 *
 *  HOW TO CALIBRATE:
 *    1. Take the device to clean air.
 *    2. Power it on while holding the PA1 button.
 *    3. Wait 5 minutes warm-up (MQ-9 needs this always).
 *    4. Device auto-calibrates → saves R0 to flash.
 *    5. Triple beep = done. Calibration is stored forever.
 *    6. From now on, just power on normally — no button.
 *
 *  YOU ONLY CALIBRATE ONCE (or when you replace the sensor).
 *  After calibration, just power on and read PPM directly.
 *
 *  ── DISPLAY ─────────────────────────────────────────────
 *  Simple, clean. No Rs, no R0, no debug screens.
 *
 *    Normal:            Alarm:
 *    ┌────────────────┐  ┌────────────────┐
 *    │CO:   12 PPM    │  │CO:  165 PPM    │
 *    │LPG:   4 PPM    │  │CO  !!! ALARM ! │
 *    └────────────────┘  └────────────────┘
 *
 *  ── KH158 ALARM THRESHOLDS ──────────────────────────────
 *  CO  alarm: 150 ppm  (KH158 official spec)
 *  CO  clear: 140 ppm
 *  LPG alarm: 2500 ppm (KH158: 5% LEL)
 *  LPG clear: 2300 ppm
 *
 *  ── KH158 ALARM PATTERN ─────────────────────────────────
 *  Phase 1 (first 4 min): 4 beeps → 5s silence → repeat
 *  Phase 2 (after 4 min): 4 beeps → 56s silence → repeat
 *  All-clear: 1 long beep
 *
 *  ── WIRING ──────────────────────────────────────────────
 *  PA0  → MQ-9 AOUT voltage divider:
 *         MQ9-AOUT ── 10kΩ ── PA0 ── 10kΩ ── GND
 *  PA1  → Button to GND
 *           Hold at power-on = force recalibration
 *           Press while running = self-test
 *  PA2  → 1kΩ → NPN base (2N2222/BC547)
 *         NPN collector → Buzzer+ → 5V
 *         NPN emitter → GND
 *  PA3  → 330Ω → Red LED → GND  (alarm flash)
 *  PB6  → LCD SCL
 *  PB7  → LCD SDA
 *  5V   → MQ-9 VCC, LCD VCC, buzzer rail
 *  GND  → MQ-9 GND, LCD GND, NPN emitter
 *
 * ============================================================
 */

#include <stdint.h>
#include <math.h>

/* ── Registers ─────────────────────────────────────────────── */
#define FLASH_ACR    (*(volatile uint32_t*)0x40022000UL)
#define FLASH_KEYR   (*(volatile uint32_t*)0x40022004UL)
#define FLASH_SR     (*(volatile uint32_t*)0x4002200CUL)
#define FLASH_CR     (*(volatile uint32_t*)0x40022010UL)
#define FLASH_AR     (*(volatile uint32_t*)0x40022014UL)
#define RCC_CR       (*(volatile uint32_t*)0x40021000UL)
#define RCC_CFGR     (*(volatile uint32_t*)0x40021004UL)
#define RCC_APB2ENR  (*(volatile uint32_t*)0x40021018UL)
#define RCC_APB1ENR  (*(volatile uint32_t*)0x4002101CUL)
#define AFIO_MAPR    (*(volatile uint32_t*)0x40010004UL)
#define GPIOA_CRL    (*(volatile uint32_t*)0x40010800UL)
#define GPIOA_IDR    (*(volatile uint32_t*)0x40010808UL)
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

/* ── I2C pins ──────────────────────────────────────────────── */
#define SCL_HIGH()    GPIOB_BSRR=(1U<<6)
#define SCL_LOW()     GPIOB_BRR =(1U<<6)
#define SDA_HIGH()    GPIOB_BSRR=(1U<<7)
#define SDA_LOW()     GPIOB_BRR =(1U<<7)
#define SDA_READ()    ((GPIOB_IDR>>7)&1U)

/* ── Output helpers ────────────────────────────────────────── */
#define BUZZER_ON()   GPIOA_BSRR=(1U<<2)
#define BUZZER_OFF()  GPIOA_BRR =(1U<<2)
#define LED_ON()      GPIOA_BSRR=(1U<<3)
#define LED_OFF()     GPIOA_BRR =(1U<<3)
#define BTN_PRESSED() (((GPIOA_IDR>>1)&1U)==0U)

/* ── Flash storage ─────────────────────────────────────────── */
#define FLASH_PAGE63   0x0800F800UL
#define CALIB_MAGIC    0xCAFE1234UL   /* change this to force re-calib */

/* ── Sensor hardware constants ─────────────────────────────── */
#define MQ9_RL          10000.0f   /* load resistor = 10 kΩ */
#define MQ9_VSUPPLY     5.0f       /* sensor supply = 5V */
#define MQ9_VREF        3.3f       /* STM32 ADC reference = 3.3V */
#define MQ9_ADC_MAX     4095.0f    /* 12-bit ADC */
#define ADC_DIVIDER     2.0f       /* 10k+10k divider → multiply by 2 */

/*
 * MQ-9 sensitivity curve constants from datasheet Figure 2.
 * Formula: PPM = A × (Rs/R0) ^ B
 *
 * CO:  A=100, B=-1.513
 * LPG: A=4.4, B=-1.265
 *
 * These are fixed — they come from the datasheet, not calibration.
 * Only R0 comes from your calibration.
 */
#define CO_A     100.0f
#define CO_B    (-1.513f)
#define LPG_A    4.4f
#define LPG_B   (-1.265f)

/*
 * R0 default — used ONLY before first calibration.
 * 10 kΩ is a reasonable middle-of-range starting point.
 * It will be replaced with your real value after calibration.
 */
#define R0_DEFAULT   10000.0f

/* ── Warm-up ───────────────────────────────────────────────── */
#define WARMUP_SEC   300U   /* 5 minutes — MQ-9 datasheet requirement */

/* ── KH158 alarm thresholds ────────────────────────────────── */
#define CO_ALARM_PPM    150.0f
#define CO_CLEAR_PPM    140.0f
#define LPG_ALARM_PPM   2500.0f
#define LPG_CLEAR_PPM   2300.0f

/* ── KH158 alarm timing ────────────────────────────────────── */
#define PHASE1_DURATION_MS   240000UL   /* 4 minutes in phase 1 */
#define PHASE1_SILENCE_MS      5000UL   /* 5s gap between bursts */
#define PHASE2_SILENCE_MS     56000UL   /* 56s gap in phase 2 */
#define BEEP_ON_MS               100U   /* each beep length */
#define BEEP_OFF_MS              100U   /* gap between beeps */

/* ── Normal heartbeat LED ──────────────────────────────────── */
#define HEARTBEAT_INTERVAL_MS  30000UL  /* flash once per 30 seconds */
#define HEARTBEAT_FLASH_MS        50U

/* ── Buzzer frequency (~3000 Hz at 64 MHz) ─────────────────── */
#define BUZZ_HALF      2667U

/* ── LCD ───────────────────────────────────────────────────── */
#define LCD_ADDR   0x27U
#define BL  0x08U
#define EN  0x04U
#define RS  0x01U

/* ═══════════════════════════════════════════════════════════
 * GLOBALS
 * ═══════════════════════════════════════════════════════════ */
static volatile uint32_t g_ms = 0;
static float g_R0 = R0_DEFAULT;
static uint8_t g_lcd = LCD_ADDR;

/* ── SysTick ───────────────────────────────────────────────── */
void SysTick_Handler(void) { g_ms++; }

static void delay_ms(uint32_t ms)
{ uint32_t s = g_ms; while((g_ms - s) < ms); }

static void delay_us(uint32_t us)
{ volatile uint32_t n = us * 8U; while(n--); }

/* ═══════════════════════════════════════════════════════════
 * BUZZER
 * ═══════════════════════════════════════════════════════════ */
static void buzz_ms(uint32_t ms)
{
    uint32_t pairs = (ms * 8000UL) / BUZZ_HALF;
    for(uint32_t i = 0; i < pairs; i++){
        BUZZER_ON();
        volatile uint32_t n = BUZZ_HALF; while(n--);
        BUZZER_OFF();
        n = BUZZ_HALF; while(n--);
    }
    BUZZER_OFF();
}

/* 4-beep burst: exactly what KH158 does each alarm cycle */
static void alarm_burst(void)
{
    for(uint8_t i = 0; i < 4U; i++){
        LED_ON();
        buzz_ms(BEEP_ON_MS);
        LED_OFF();
        delay_ms(BEEP_OFF_MS);
    }
}

static void beep_startup(void)   { buzz_ms(200); delay_ms(100); }
static void beep_allclear(void)  { buzz_ms(800); }
static void beep_calibdone(void) {
    for(uint8_t i=0;i<3;i++){ buzz_ms(200); delay_ms(150); }
}

/* ═══════════════════════════════════════════════════════════
 * CLOCK — 64 MHz
 * ═══════════════════════════════════════════════════════════ */
static void clock_init(void)
{
    FLASH_ACR = (1U<<4)|2U;
    RCC_CR |= 1U; while(!(RCC_CR&(1U<<1)));
    RCC_CFGR = (4U<<8)|(3U<<14)|(0U<<16)|(14U<<18);
    RCC_CR |= (1U<<24); while(!(RCC_CR&(1U<<25)));
    RCC_CFGR |= 2U;
    while((RCC_CFGR&(3U<<2))!=(2U<<2));
}

/* ═══════════════════════════════════════════════════════════
 * FLASH
 * ═══════════════════════════════════════════════════════════ */
static void flash_unlock(void) {
    if(FLASH_CR&(1U<<7)){
        FLASH_KEYR=0x45670123UL; FLASH_KEYR=0xCDEF89ABUL;
    }
}
static void flash_lock(void)  { FLASH_CR|=(1U<<7); }
static void flash_wait(void)  { while(FLASH_SR&1U); }

static void flash_erase_page(uint32_t addr) {
    flash_unlock(); flash_wait();
    FLASH_CR|=(1U<<1); FLASH_AR=addr;
    FLASH_CR|=(1U<<6); flash_wait();
    FLASH_CR&=~(1U<<1);
}
static void flash_write16(uint32_t addr, uint16_t val) {
    flash_unlock(); flash_wait();
    FLASH_CR|=(1U<<0);
    *(volatile uint16_t*)addr=val;
    flash_wait(); FLASH_CR&=~(1U<<0);
}
static void flash_write32(uint32_t addr, uint32_t val) {
    flash_write16(addr,   (uint16_t)(val&0xFFFFU));
    flash_write16(addr+2U,(uint16_t)(val>>16));
}

/*
 * Load saved R0 from flash.
 * Returns 0 if no valid calibration found.
 */
static float flash_load_r0(void) {
    uint32_t magic = *(volatile uint32_t*)(FLASH_PAGE63);
    if(magic != CALIB_MAGIC) return 0.0f;
    uint32_t bits  = *(volatile uint32_t*)(FLASH_PAGE63+4U);
    float r0;
    uint8_t *s=(uint8_t*)&bits, *d=(uint8_t*)&r0;
    for(uint8_t i=0;i<4;i++) d[i]=s[i];
    return r0;
}

static void flash_save_r0(float r0) {
    uint32_t bits;
    uint8_t *s=(uint8_t*)&r0, *d=(uint8_t*)&bits;
    for(uint8_t i=0;i<4;i++) d[i]=s[i];
    flash_erase_page(FLASH_PAGE63);
    flash_write32(FLASH_PAGE63,    CALIB_MAGIC);
    flash_write32(FLASH_PAGE63+4U, bits);
    flash_lock();
}

/* ═══════════════════════════════════════════════════════════
 * SOFTWARE I2C
 * ═══════════════════════════════════════════════════════════ */
static void i2c_init(void) {
    GPIOB_CRL &= ~(0xFFU<<24);
    GPIOB_CRL |=  (0x6U<<24)|(0x6U<<28);
    SCL_HIGH(); SDA_HIGH(); delay_ms(10);
}
static void i2c_start(void) {
    SDA_HIGH(); delay_us(5); SCL_HIGH(); delay_us(5);
    SDA_LOW();  delay_us(5); SCL_LOW();  delay_us(5);
}
static void i2c_stop(void) {
    SDA_LOW(); delay_us(5); SCL_HIGH(); delay_us(5); SDA_HIGH(); delay_us(5);
}
static uint8_t i2c_write_byte(uint8_t b) {
    for(uint8_t i=0;i<8;i++){
        if(b&0x80U) SDA_HIGH(); else SDA_LOW();
        delay_us(3); SCL_HIGH(); delay_us(5); SCL_LOW(); delay_us(3);
        b<<=1;
    }
    SDA_HIGH(); delay_us(3); SCL_HIGH(); delay_us(5);
    uint8_t ack=(uint8_t)(1U-SDA_READ());
    SCL_LOW(); delay_us(3);
    return ack;
}
static void i2c_send(uint8_t addr, uint8_t data) {
    i2c_start();
    if(i2c_write_byte((uint8_t)(addr<<1))) i2c_write_byte(data);
    i2c_stop(); delay_us(100);
}
static uint8_t i2c_probe(uint8_t addr) {
    i2c_start();
    uint8_t ack=i2c_write_byte((uint8_t)(addr<<1));
    i2c_stop(); delay_ms(1);
    return ack;
}

/* ═══════════════════════════════════════════════════════════
 * LCD (16×2, I2C backpack)
 * ═══════════════════════════════════════════════════════════ */
static void lcd_pulse(uint8_t n) {
    i2c_send(g_lcd,(uint8_t)(n|EN));  delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(n&~EN)); delay_ms(2);
}
static void lcd_nibble(uint8_t v, uint8_t mode) {
    lcd_pulse((uint8_t)((v&0xF0U)|BL|mode));
    lcd_pulse((uint8_t)(((v<<4)&0xF0U)|BL|mode));
}
static void lcd_cmd(uint8_t c) { lcd_nibble(c,0);   }
static void lcd_chr(uint8_t c) { lcd_nibble(c,RS);  }
static void lcd_str(const char *s) { while(*s) lcd_chr((uint8_t)*s++); }
static void lcd_clear(void) { lcd_cmd(0x01); delay_ms(5); }
static void lcd_pos(uint8_t col, uint8_t row) {
    lcd_cmd(0x80U|(uint8_t)(col+(row?0x40U:0U)));
}
static void lcd_init(void) {
    delay_ms(200);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL|EN)); delay_ms(5);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL));    delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL|EN)); delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL));    delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL|EN)); delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x30U|BL));    delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x20U|BL|EN)); delay_ms(2);
    i2c_send(g_lcd,(uint8_t)(0x20U|BL));    delay_ms(2);
    lcd_cmd(0x28); delay_ms(2);
    lcd_cmd(0x08); delay_ms(2);
    lcd_cmd(0x01); delay_ms(5);
    lcd_cmd(0x06); delay_ms(2);
    lcd_cmd(0x0C); delay_ms(2);
}

/* Print an unsigned integer, right-aligned in width w */
static void lcd_num(uint32_t v, uint8_t w) {
    char buf[12]; uint8_t i=0;
    if(v==0U){ buf[i++]='0'; }
    else { while(v>0U){ buf[i++]=(char)('0'+(v%10U)); v/=10U; } }
    while(i<w) buf[i++]=' ';
    for(uint8_t a=0,b=(uint8_t)(i-1);a<b;a++,b--)
    { char t=buf[a]; buf[a]=buf[b]; buf[b]=t; }
    buf[i]='\0'; lcd_str(buf);
}

/* ═══════════════════════════════════════════════════════════
 * ADC
 * ═══════════════════════════════════════════════════════════ */
static void adc_init(void) {
    GPIOA_CRL &= ~(0xFU<<0);         /* PA0 = analog input */
    ADC1_SMPR2  = (7U<<0);           /* longest sample time */
    ADC1_SQR1=0U; ADC1_SQR3=0U;
    ADC1_CR2=(7U<<17)|(1U<<20);
    ADC1_CR2|=(1U<<0); delay_ms(5);
    ADC1_CR2|=(1U<<3); while(ADC1_CR2&(1U<<3));
    ADC1_CR2|=(1U<<2); while(ADC1_CR2&(1U<<2));
}

static uint16_t adc_single(void) {
    ADC1_SR &=~(1U<<1);
    ADC1_CR2|= (1U<<22);
    while(!(ADC1_SR&(1U<<1)));
    return (uint16_t)(ADC1_DR&0xFFFU);
}

/* Average 64 samples → stable reading */
static uint16_t adc_avg(void) {
    uint32_t sum=0;
    for(uint8_t i=0;i<64;i++){ sum+=adc_single(); delay_ms(5); }
    return (uint16_t)(sum/64U);
}

/* ═══════════════════════════════════════════════════════════
 * SENSOR MATH
 * ═══════════════════════════════════════════════════════════ */

/*
 * adc_to_rs() — convert raw ADC count to sensor resistance Rs.
 *
 * Step 1: raw ADC → voltage at PA0 pin (0 to 3.3V)
 * Step 2: × divider ratio → actual sensor output voltage (0 to 5V)
 * Step 3: Rs = RL × (Vsupply − Vsensor) / Vsensor
 *
 * Rs is high in clean air, low when gas is present.
 */
static float adc_to_rs(uint16_t raw) {
    float v_pin    = ((float)raw / MQ9_ADC_MAX) * MQ9_VREF;
    float v_sensor = v_pin * ADC_DIVIDER;
    if(v_sensor < 0.01f)                v_sensor = 0.01f;
    if(v_sensor > MQ9_VSUPPLY - 0.01f)  v_sensor = MQ9_VSUPPLY - 0.01f;
    return MQ9_RL * (MQ9_VSUPPLY - v_sensor) / v_sensor;
}

/*
 * rs_to_ppm() — convert Rs to gas concentration in PPM.
 *
 * Formula:  PPM = A × (Rs / R0) ^ B
 *
 * When Rs = R0 (same as clean air) → ratio = 1 → PPM = A × 1 = A
 * When Rs < R0 (gas present)       → ratio < 1 → PPM rises
 *
 * This is why R0 MUST be measured in clean air.
 * If R0 is wrong, every PPM value is wrong.
 */
static float rs_to_ppm(float rs, float A, float B) {
    float ratio = rs / g_R0;
    if(ratio < 0.001f) ratio = 0.001f;
    if(ratio > 100.0f) ratio = 100.0f;
    float ppm = A * powf(ratio, B);
    if(ppm < 0.0f)    ppm = 0.0f;
    if(ppm > 9999.0f) ppm = 9999.0f;
    return ppm;
}

static void measure(float *co, float *lpg) {
    float rs = adc_to_rs(adc_avg());
    *co  = rs_to_ppm(rs, CO_A,  CO_B);
    *lpg = rs_to_ppm(rs, LPG_A, LPG_B);
}

/* ═══════════════════════════════════════════════════════════
 * CLEAN-AIR CALIBRATION
 *
 * This is the CORRECT way to calibrate the MQ-9:
 *
 *   1. Take 100 ADC samples in CLEAN AIR.
 *   2. Convert each to Rs using the voltage divider formula.
 *   3. Average all valid Rs readings.
 *   4. That average IS your R0 — resistance in clean air.
 *   5. Save R0 to flash. Done forever.
 *
 * NO ASSUMPTIONS about ambient PPM.
 * NO BASELINE_CO_PPM guessing.
 * Just: clean air → measure → that IS R0.
 *
 * Valid Rs range for MQ-9 in clean air: 5 kΩ – 50 kΩ
 * (from datasheet; outside this range = sensor not ready
 *  or environment is not clean air)
 * ═══════════════════════════════════════════════════════════ */
static void do_calibration(void) {
    float   sum   = 0.0f;
    uint8_t count = 0;

    for(uint8_t i = 0; i < 100U; i++) {
        float rs = adc_to_rs(adc_single());
        /*
         * Accept only readings in the clean-air Rs range.
         * 5 kΩ–50 kΩ is what the MQ-9 datasheet shows for
         * Rs in clean air at normal temperature/humidity.
         */
        if(rs > 5000.0f && rs < 50000.0f) {
            sum += rs;
            count++;
        }
        delay_ms(100);
    }

    if(count >= 80U) {
        /*
         * Good calibration: enough clean-air samples collected.
         * The average Rs in clean air IS our R0.
         * Save it to flash — it stays there until next calibration.
         */
        g_R0 = sum / (float)count;
    } else {
        /*
         * Not enough valid samples.
         * Either sensor is not warmed up, or air is not clean enough.
         * Keep default R0 — device will show a CALIB ERROR message.
         */
        g_R0 = R0_DEFAULT;
    }

    flash_save_r0(g_R0);
}

/* ═══════════════════════════════════════════════════════════
 * LCD SCREENS
 *
 * Simple, clean display — PPM values only.
 * No Rs, no R0, no ratio. Just what the user needs.
 * ═══════════════════════════════════════════════════════════ */

/* Normal screen: live CO and LPG in PPM */
static void screen_normal(float co, float lpg) {
    /*
     * Line 1: CO:  XXXX PPM
     * Line 2: LPG: XXXX PPM
     */
    uint32_t co_int  = (uint32_t)(co  > 9999.0f ? 9999.0f : co);
    uint32_t lpg_int = (uint32_t)(lpg > 9999.0f ? 9999.0f : lpg);

    lcd_pos(0,0); lcd_str("CO:  ");
    lcd_num(co_int, 4);
    lcd_str(" PPM");

    lcd_pos(0,1); lcd_str("LPG: ");
    lcd_num(lpg_int, 4);
    lcd_str(" PPM");
}

/* Alarm screen: shows which gas is alarming */
static void screen_alarm(float co, float lpg, uint8_t co_alarm, uint8_t lpg_alarm) {
    uint32_t co_int  = (uint32_t)(co  > 9999.0f ? 9999.0f : co);
    uint32_t lpg_int = (uint32_t)(lpg > 9999.0f ? 9999.0f : lpg);

    lcd_pos(0,0); lcd_str("CO:");
    lcd_num(co_int, 4);
    lcd_str(co_alarm ? "p !ALRM!" : "p  OK   ");

    lcd_pos(0,1); lcd_str("LPG:");
    lcd_num(lpg_int, 4);
    lcd_str(lpg_alarm ? " !ALRM!" : "  OK   ");
}

/* Warm-up countdown screen */
static void screen_warmup(uint32_t sec_left) {
    lcd_pos(0,0); lcd_str("  CO MONITOR    ");
    lcd_pos(0,1); lcd_str("WARMUP: ");
    lcd_num(sec_left, 3);
    lcd_chr('s'); lcd_str("    ");
}

/* ═══════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════ */
typedef enum { ST_WARMUP, ST_CALIB, ST_MONITOR, ST_ALARM } State_t;

int main(void)
{
    clock_init();

    /* Enable clocks: AFIO, GPIOA, GPIOB, ADC1 */
    RCC_APB2ENR |= (1U<<0)|(1U<<2)|(1U<<3)|(1U<<9);

    /* Disable JTAG (keep SWD), free PA15/PB3/PB4 */
    AFIO_MAPR = (AFIO_MAPR & ~(7U<<24)) | (2U<<24);

    /* PA1: input pull-up (button) */
    GPIOA_CRL &= ~(0xFU<<4);
    GPIOA_CRL |=  (0x8U<<4);
    GPIOA_BSRR = (1U<<1);

    /* PA2: output 50MHz (buzzer NPN base) */
    GPIOA_CRL &= ~(0xFU<<8);
    GPIOA_CRL |=  (0x3U<<8);
    BUZZER_OFF();

    /* PA3: output 50MHz (red alarm LED) */
    GPIOA_CRL &= ~(0xFU<<12);
    GPIOA_CRL |=  (0x3U<<12);
    LED_OFF();

    /* SysTick: 1ms interrupt at 64 MHz */
    STK_LOAD_R = 64000U - 1U;
    STK_VAL    = 0U;
    STK_CTRL   = 7U;

    delay_ms(300);
    i2c_init();
    adc_init();

    /* Find LCD (try 0x27 then 0x3F) */
    while(1){
        if(i2c_probe(0x27U)){ g_lcd=0x27U; break; }
        delay_ms(100);
        if(i2c_probe(0x3FU)){ g_lcd=0x3FU; break; }
        delay_ms(500);
    }

    lcd_init();
    lcd_clear();
    lcd_pos(0,0); lcd_str("  CO MONITOR    ");
    lcd_pos(0,1); lcd_str("   STARTING...  ");
    beep_startup();   /* 1 beep: device on */

    /* ── Decide: normal boot or force recalibration ── */
    float   saved_r0     = flash_load_r0();
    uint8_t force_recal  = (uint8_t)(BTN_PRESSED());

    State_t  state;
    uint32_t warmup_start   = g_ms;
    uint32_t last_lcd_ms    = g_ms;
    uint32_t last_hb_ms     = g_ms;   /* heartbeat LED */
    uint32_t alarm_start_ms = 0UL;
    uint32_t last_burst_ms  = 0UL;
    float    co = 0.0f, lpg = 0.0f;
    uint8_t  calib_failed   = 0U;

    if(!force_recal && saved_r0 > 500.0f && saved_r0 < 100000.0f){
        /*
         * Valid calibration found in flash.
         * Go straight to monitoring — no warm-up needed
         * because the sensor warms up during startup.
         * (We still wait 30s before first reading for stability.)
         */
        g_R0  = saved_r0;
        state = ST_MONITOR;
        lcd_clear();
        lcd_pos(0,0); lcd_str("  CO MONITOR    ");
        lcd_pos(0,1); lcd_str("   READY        ");
        delay_ms(2000);
        lcd_clear();
        screen_normal(0.0f, 0.0f);
    } else {
        /* No calibration saved, or button held → warm-up + calibrate */
        g_R0  = R0_DEFAULT;
        state = ST_WARMUP;
        calib_failed = 0U;
        lcd_clear();
        lcd_pos(0,0); lcd_str("  CO MONITOR    ");
        if(force_recal){
            lcd_pos(0,1); lcd_str(" RECALIB MODE   ");
            beep_calibdone();   /* 3 beeps = entering recal */
        } else {
            lcd_pos(0,1); lcd_str(" FIRST SETUP    ");
            /*
             * IMPORTANT MESSAGE FOR FIRST BOOT:
             * Show user they need to be in clean air.
             */
            delay_ms(2000);
            lcd_clear();
            lcd_pos(0,0); lcd_str("PLACE IN CLEAN  ");
            lcd_pos(0,1); lcd_str("AIR NOW!        ");
        }
        delay_ms(3000);
        lcd_clear();
    }

    /* ═══════════════════════════════════════════════════════
     * MAIN LOOP
     * ═══════════════════════════════════════════════════════ */
    while(1)
    {
        /* ─────────────────────────────────────────────────
         * STATE: WARMUP
         * Count down 5 minutes. MQ-9 datasheet requires
         * this every time after cold power-on.
         * After warm-up → go to calibration.
         * ───────────────────────────────────────────────── */
        if(state == ST_WARMUP)
        {
            uint32_t elapsed = (g_ms - warmup_start) / 1000U;
            if((g_ms - last_lcd_ms) >= 1000U){
                last_lcd_ms = g_ms;
                uint32_t left = (elapsed < WARMUP_SEC)
                                ? (WARMUP_SEC - elapsed) : 0U;
                screen_warmup(left);
            }
            if(elapsed >= WARMUP_SEC)
                state = ST_CALIB;
        }

        /* ─────────────────────────────────────────────────
         * STATE: CALIB
         *
         * Measures Rs in clean air → stores as R0.
         *
         * If calibration fails (not enough good samples),
         * shows "CALIB FAIL — NOT CLEAN AIR" and uses
         * default R0. The user should move to cleaner air
         * and recalibrate by pressing the button at boot.
         * ───────────────────────────────────────────────── */
        else if(state == ST_CALIB)
        {
            lcd_clear();
            lcd_pos(0,0); lcd_str("  CALIBRATING   ");
            lcd_pos(0,1); lcd_str("  PLEASE WAIT   ");

            float rs_before = adc_to_rs(adc_avg());
            do_calibration();

            /*
             * Check if calibration produced a sensible R0.
             * If g_R0 is still at default, calibration failed.
             */
            if(g_R0 == R0_DEFAULT){
                calib_failed = 1U;
                lcd_clear();
                lcd_pos(0,0); lcd_str("!CALIB FAIL!    ");
                lcd_pos(0,1); lcd_str("NOT CLEAN AIR   ");
                /* 5 rapid beeps = warning */
                for(uint8_t i=0;i<5U;i++){buzz_ms(100);delay_ms(100);}
                delay_ms(3000);
                lcd_clear();
                lcd_pos(0,0); lcd_str("USING DEFAULT   ");
                lcd_pos(0,1); lcd_str("RECALIB NEEDED  ");
                delay_ms(2000);
            } else {
                calib_failed = 0U;
                lcd_clear();
                lcd_pos(0,0); lcd_str("  CALIB DONE!   ");
                lcd_pos(0,1); lcd_str("MONITORING...   ");
                beep_calibdone();   /* 3 beeps = success */
                delay_ms(2000);
            }

            (void)rs_before;   /* suppress unused warning */
            lcd_clear();
            state       = ST_MONITOR;
            last_lcd_ms = last_hb_ms = g_ms;
            measure(&co, &lpg);
            screen_normal(co, lpg);
        }

        /* ─────────────────────────────────────────────────
         * STATE: MONITOR
         *
         * Reads sensor every 1 second.
         * Displays CO and LPG in PPM — nothing else.
         * LED heartbeat every 30 seconds (device alive).
         * PA1 button → self-test.
         * ───────────────────────────────────────────────── */
        else if(state == ST_MONITOR)
        {
            /* Update display every 1 second */
            if((g_ms - last_lcd_ms) >= 1000U){
                last_lcd_ms = g_ms;
                measure(&co, &lpg);
                screen_normal(co, lpg);

                /* Check alarm condition */
                if(co >= CO_ALARM_PPM || lpg >= LPG_ALARM_PPM){
                    lcd_clear();
                    state          = ST_ALARM;
                    alarm_start_ms = g_ms;
                    /* Fire first burst immediately */
                    last_burst_ms  = g_ms - PHASE1_SILENCE_MS;
                    uint8_t ca = (co  >= CO_ALARM_PPM)  ? 1U : 0U;
                    uint8_t la = (lpg >= LPG_ALARM_PPM) ? 1U : 0U;
                    screen_alarm(co, lpg, ca, la);
                }
            }

            /* 30-second heartbeat LED flash */
            if((g_ms - last_hb_ms) >= HEARTBEAT_INTERVAL_MS){
                last_hb_ms = g_ms;
                LED_ON();  delay_ms(HEARTBEAT_FLASH_MS);  LED_OFF();
            }

            /* Self-test on PA1 button press */
            if(BTN_PRESSED()){
                delay_ms(50);
                if(BTN_PRESSED()){
                    lcd_clear();
                    lcd_pos(0,0); lcd_str("  SELF  TEST    ");
                    lcd_pos(0,1); lcd_str("  TESTING...    ");
                    /* 3 alarm bursts = full test */
                    for(uint8_t t=0;t<3U;t++){
                        alarm_burst();
                        delay_ms(800U);
                    }
                    while(BTN_PRESSED()) delay_ms(10);
                    lcd_clear();
                    measure(&co, &lpg);
                    screen_normal(co, lpg);
                    last_lcd_ms = last_hb_ms = g_ms;
                }
            }
        }

        /* ─────────────────────────────────────────────────
         * STATE: ALARM
         *
         * KH158 exact pattern:
         *   Phase 1 (0–4 min): 4 beeps → 5s gap → repeat
         *   Phase 2 (4+ min):  4 beeps → 56s gap → repeat
         *
         * LED flashes with each beep.
         * Display updates every second with live PPM.
         *
         * Clears when: CO < 140 AND LPG < 2300
         * On clear: 1 long beep + "GAS CLEARED" message.
         * ───────────────────────────────────────────────── */
        else if(state == ST_ALARM)
        {
            uint32_t alarm_elapsed = g_ms - alarm_start_ms;
            uint32_t gap = (alarm_elapsed < PHASE1_DURATION_MS)
                           ? PHASE1_SILENCE_MS
                           : PHASE2_SILENCE_MS;

            /* Time to fire the next burst? */
            if((g_ms - last_burst_ms) >= gap){
                last_burst_ms = g_ms;

                /* Play 4-beep burst with LED flash */
                alarm_burst();

                /* Read fresh values immediately after burst */
                measure(&co, &lpg);

                /* Check for all-clear */
                if(co < CO_CLEAR_PPM && lpg < LPG_CLEAR_PPM){
                    /* Gas gone — sound all-clear and return to monitor */
                    LED_OFF();
                    BUZZER_OFF();
                    beep_allclear();   /* 1 long beep = all clear */

                    lcd_clear();
                    lcd_pos(0,0); lcd_str("  GAS CLEARED   ");
                    lcd_pos(0,1); lcd_str("  SAFE NOW      ");
                    delay_ms(3000);

                    lcd_clear();
                    state       = ST_MONITOR;
                    last_lcd_ms = last_hb_ms = g_ms;
                    screen_normal(co, lpg);
                } else {
                    /* Still alarming — update display */
                    uint8_t ca = (co  >= CO_ALARM_PPM)  ? 1U : 0U;
                    uint8_t la = (lpg >= LPG_ALARM_PPM) ? 1U : 0U;
                    screen_alarm(co, lpg, ca, la);
                }
            }

            /* Update display every second between bursts */
            if((g_ms - last_lcd_ms) >= 1000U){
                last_lcd_ms = g_ms;
                measure(&co, &lpg);
                uint8_t ca = (co  >= CO_ALARM_PPM)  ? 1U : 0U;
                uint8_t la = (lpg >= LPG_ALARM_PPM) ? 1U : 0U;
                screen_alarm(co, lpg, ca, la);
            }
        }

    } /* while(1) */

    return 0;
}