/*
 * ============================================================
 *  MQ-9 CO / LPG GAS MONITOR — PERSISTENT BURN-IN TIMER
 *  FIXED: Timer now survives power loss
 *  Target  : STM32F103C8T6 (Blue Pill)
 *  Method  : Pure Bare Metal — zero HAL
 * ============================================================
 *
 *  platformio.ini:
 *  [env:genericSTM32F103C8]
 *  platform  = ststm32
 *  board     = genericSTM32F103C8
 *  framework = cmsis
 *  build_flags = -lm
 *
 *  WIRING:
 *  PA0  → MQ-9 A_OUT through voltage divider: AOUT--10k--PA0--20k--GND
 *  PA2  → Buzzer (+) direct
 *  GND  → Buzzer (−)
 *  PB6  → LCD SCL
 *  PB7  → LCD SDA
 *  5V   → MQ-9 VCC + LCD VCC
 *  GND  → MQ-9 GND + LCD GND
 */

#include <stdint.h>
#include <math.h>

/* ── Registers ──────────────────────────────────────────────────── */
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

/* ── Software I2C ───────────────────────────────────────────────── */
#define SCL_HIGH()  GPIOB_BSRR=(1U<<6)
#define SCL_LOW()   GPIOB_BRR =(1U<<6)
#define SDA_HIGH()  GPIOB_BSRR=(1U<<7)
#define SDA_LOW()   GPIOB_BRR =(1U<<7)
#define SDA_READ()  ((GPIOB_IDR>>7)&1U)

/* ============================================================
 * PROJECT SETTINGS
 * ============================================================ */

#define CALIB_MAGIC             0xBEEF4805UL
#define BURN_MAGIC              0xB00B4805UL

#define FLASH_PAGE63            0x0800F800UL  /* R0 calibration storage page */
#define FLASH_PAGE62            0x0800F400UL  /* burn-in progress storage page */

/*
 * Save burn-in progress every 1 minute.
 * Do not save every second because STM32 Flash has limited erase/write life.
 */
#define BURN_SAVE_INTERVAL_SEC  60U

/*
 * You said sensor already ran 5 hours.
 * 5 hours = 5 * 60 * 60 = 18000 seconds.
 */
#define INITIAL_BURN_DONE_SEC   18000U

#define CO_ALARM_PPM            200.0f
#define LPG_ALARM_PPM           1000.0f

#define WARMUP_FIRST_SEC        172800U   /* 48 hours */
#define WARMUP_NORMAL_SEC       300U      /* 5 minutes after calibration exists */

/*
 * Voltage divider:
 * MQ-9 AOUT ---- 10k ---- PA0 ---- 20k ---- GND
 * PA0 gets 2/3 voltage, so original MQ voltage = PA0 voltage * 3
 */
#define ADC_DIVIDER_RATIO       3.0f

#define MQ9_RL                  10000.0f
#define MQ9_VSUPPLY             5.0f
#define MQ9_VREF                3.3f
#define MQ9_ADC_MAX             4095.0f
#define MQ9_R0_DEFAULT          192102.0f

#define CO_A                    100.0f
#define CO_B                    (-1.513f)
#define LPG_A                   4.4f
#define LPG_B                   (-1.265f)

#define LCD_ADDR_DEFAULT        0x27U

/* ============================================================
 * SYSTICK
 * ============================================================ */
static volatile uint32_t g_ms    = 0;
static volatile uint32_t g_tick4 = 0;
static volatile uint8_t  g_buz_en = 0;
static volatile uint32_t g_buz_c = 0;
static volatile uint8_t  g_buz_p = 0;

void SysTick_Handler(void){
    g_tick4++;

    if((g_tick4 & 3U) == 0U){
        g_ms++;
    }

    if(g_buz_en){
        if(++g_buz_c >= 2U){
            g_buz_c = 0;
            g_buz_p ^= 1U;

            if(g_buz_p){
                GPIOA_BSRR = (1U << 2);
            }else{
                GPIOA_BRR = (1U << 2);
            }
        }
    }else{
        GPIOA_BRR = (1U << 2);
        g_buz_p = 0;
        g_buz_c = 0;
    }
}

static void delay_ms(uint32_t ms){
    uint32_t s = g_ms;
    while((g_ms - s) < ms);
}

static void delay_us(uint32_t us){
    volatile uint32_t n = us * 8U;
    while(n--);
}

static void buzzer_on(void){
    g_buz_en = 1;
}

static void buzzer_off(void){
    g_buz_en = 0;
}

/* ============================================================
 * CLOCK — 64 MHz
 * ============================================================ */
static void clock_init(void){
    FLASH_ACR = (1U << 4) | 2U;

    RCC_CR |= (1U << 0);
    while(!(RCC_CR & (1U << 1)));

    RCC_CFGR = (4U << 8) | (2U << 14) | (0U << 16) | (14U << 18);

    RCC_CR |= (1U << 24);
    while(!(RCC_CR & (1U << 25)));

    RCC_CFGR |= 2U;
    while((RCC_CFGR & (3U << 2)) != (2U << 2));
}

/* ============================================================
 * FLASH FUNCTIONS
 * Page63: saved R0
 * Page62: saved completed burn-in seconds
 * ============================================================ */
static void flash_unlock_if_needed(void){
    if(FLASH_CR & (1U << 7)){
        FLASH_KEYR = 0x45670123UL;
        FLASH_KEYR = 0xCDEF89ABUL;
    }
}

static void flash_lock(void){
    FLASH_CR |= (1U << 7);
}

static void flash_erase_page(uint32_t page_addr){
    flash_unlock_if_needed();

    while(FLASH_SR & (1U << 0));

    FLASH_CR |= (1U << 1);
    FLASH_AR = page_addr;
    FLASH_CR |= (1U << 6);

    while(FLASH_SR & (1U << 0));

    FLASH_CR &= ~(1U << 1);
}

static void flash_write_half(uint32_t addr, uint16_t val){
    flash_unlock_if_needed();

    while(FLASH_SR & (1U << 0));

    FLASH_CR |= (1U << 0);
    *(volatile uint16_t*)addr = val;

    while(FLASH_SR & (1U << 0));

    FLASH_CR &= ~(1U << 0);
}

static void flash_write_u32(uint32_t addr, uint32_t val){
    flash_write_half(addr,     (uint16_t)(val & 0xFFFFU));
    flash_write_half(addr + 2U,(uint16_t)(val >> 16));
}

static float flash_read_r0(void){
    uint32_t magic = *(volatile uint32_t*)(FLASH_PAGE63);
    uint32_t bits  = *(volatile uint32_t*)(FLASH_PAGE63 + 4U);

    if(magic != CALIB_MAGIC){
        return 0.0f;
    }

    float r0;
    uint8_t *s = (uint8_t*)&bits;
    uint8_t *d = (uint8_t*)&r0;

    for(uint8_t i = 0; i < 4; i++){
        d[i] = s[i];
    }

    return r0;
}

static void flash_save_r0(float r0){
    uint32_t bits;
    uint8_t *s = (uint8_t*)&r0;
    uint8_t *d = (uint8_t*)&bits;

    for(uint8_t i = 0; i < 4; i++){
        d[i] = s[i];
    }

    flash_erase_page(FLASH_PAGE63);
    flash_write_u32(FLASH_PAGE63, CALIB_MAGIC);
    flash_write_u32(FLASH_PAGE63 + 4U, bits);
    flash_lock();
}

/*
 * ============================================================
 * IMPROVED BURN-IN TIME TRACKING
 * 
 * Now saves ONLY the accumulated total seconds.
 * No more session_sec + wu_start confusion.
 * ============================================================
 */
static uint32_t flash_read_burn_seconds(void){
    uint32_t magic = *(volatile uint32_t*)(FLASH_PAGE62);
    uint32_t sec   = *(volatile uint32_t*)(FLASH_PAGE62 + 4U);
    uint32_t inv   = *(volatile uint32_t*)(FLASH_PAGE62 + 8U);

    /*
     * If no saved burn progress found, start from your manual 5 hours.
     */
    if(magic != BURN_MAGIC){
        return INITIAL_BURN_DONE_SEC;
    }

    /*
     * Check saved data is valid.
     */
    if((sec ^ inv) != 0xFFFFFFFFUL){
        return INITIAL_BURN_DONE_SEC;
    }

    if(sec > WARMUP_FIRST_SEC){
        return INITIAL_BURN_DONE_SEC;
    }

    return sec;
}

static void flash_save_burn_seconds(uint32_t sec){
    if(sec > WARMUP_FIRST_SEC){
        sec = WARMUP_FIRST_SEC;
    }

    flash_erase_page(FLASH_PAGE62);
    flash_write_u32(FLASH_PAGE62, BURN_MAGIC);
    flash_write_u32(FLASH_PAGE62 + 4U, sec);
    flash_write_u32(FLASH_PAGE62 + 8U, sec ^ 0xFFFFFFFFUL);
    flash_lock();
}

static void flash_clear_burn_seconds(void){
    flash_erase_page(FLASH_PAGE62);
    flash_lock();
}

/* ============================================================
 * SOFTWARE I2C
 * ============================================================ */
static void i2c_gpio_init(void){
    GPIOB_CRL &= ~(0xFFU << 24);
    GPIOB_CRL |=  (0x6U << 24) | (0x6U << 28);

    SCL_HIGH();
    SDA_HIGH();
    delay_ms(10);
}

static void i2c_start(void){
    SDA_HIGH();
    delay_us(5);
    SCL_HIGH();
    delay_us(5);
    SDA_LOW();
    delay_us(5);
    SCL_LOW();
    delay_us(5);
}

static void i2c_stop(void){
    SDA_LOW();
    delay_us(5);
    SCL_HIGH();
    delay_us(5);
    SDA_HIGH();
    delay_us(5);
}

static uint8_t i2c_write_byte(uint8_t b){
    for(uint8_t i = 0; i < 8; i++){
        if(b & 0x80U){
            SDA_HIGH();
        }else{
            SDA_LOW();
        }

        delay_us(3);
        SCL_HIGH();
        delay_us(5);
        SCL_LOW();
        delay_us(3);

        b <<= 1;
    }

    SDA_HIGH();
    delay_us(3);
    SCL_HIGH();
    delay_us(5);

    uint8_t ack = (uint8_t)(1U - SDA_READ());

    SCL_LOW();
    delay_us(3);

    return ack;
}

static void i2c_send(uint8_t addr, uint8_t data){
    i2c_start();

    if(i2c_write_byte((uint8_t)(addr << 1))){
        i2c_write_byte(data);
    }

    i2c_stop();
    delay_us(100);
}

static uint8_t i2c_probe(uint8_t addr){
    i2c_start();

    uint8_t a = i2c_write_byte((uint8_t)(addr << 1));

    i2c_stop();
    delay_ms(1);

    return a;
}

/* ============================================================
 * LCD
 * ============================================================ */
#define BL 0x08U
#define EN 0x04U
#define RS 0x01U

static uint8_t g_lcd = LCD_ADDR_DEFAULT;

static void lcd_pulse(uint8_t n){
    i2c_send(g_lcd, (uint8_t)(n | EN));
    delay_ms(2);

    i2c_send(g_lcd, (uint8_t)(n & ~EN));
    delay_ms(2);
}

static void lcd_write(uint8_t v, uint8_t m){
    lcd_pulse((uint8_t)((v & 0xF0U) | BL | m));
    lcd_pulse((uint8_t)(((v << 4) & 0xF0U) | BL | m));
}

static void lcd_cmd(uint8_t c){
    lcd_write(c, 0);
}

static void lcd_char(uint8_t c){
    lcd_write(c, RS);
}

static void lcd_str(const char *s){
    while(*s){
        lcd_char((uint8_t)*s++);
    }
}

static void lcd_clear(void){
    lcd_cmd(0x01);
    delay_ms(5);
}

static void lcd_pos(uint8_t col, uint8_t row){
    lcd_cmd(0x80U | (uint8_t)(col + (row ? 0x40U : 0x00U)));
}

static void lcd_init(void){
    delay_ms(200);

    i2c_send(g_lcd, (uint8_t)(0x30U | BL | EN)); delay_ms(5);
    i2c_send(g_lcd, (uint8_t)(0x30U | BL));      delay_ms(2);
    i2c_send(g_lcd, (uint8_t)(0x30U | BL | EN)); delay_ms(2);
    i2c_send(g_lcd, (uint8_t)(0x30U | BL));      delay_ms(2);
    i2c_send(g_lcd, (uint8_t)(0x30U | BL | EN)); delay_ms(2);
    i2c_send(g_lcd, (uint8_t)(0x30U | BL));      delay_ms(2);
    i2c_send(g_lcd, (uint8_t)(0x20U | BL | EN)); delay_ms(2);
    i2c_send(g_lcd, (uint8_t)(0x20U | BL));      delay_ms(2);

    lcd_cmd(0x28);
    delay_ms(2);
    lcd_cmd(0x08);
    delay_ms(2);
    lcd_cmd(0x01);
    delay_ms(5);
    lcd_cmd(0x06);
    delay_ms(2);
    lcd_cmd(0x0C);
    delay_ms(2);
}

static void lcd_int(int32_t v, uint8_t w){
    char buf[12];
    uint8_t i = 0;

    uint32_t u = (v < 0) ? (uint32_t)(-v) : (uint32_t)v;

    if(u == 0U){
        buf[i++] = '0';
    }

    while(u > 0U){
        buf[i++] = (char)('0' + (u % 10U));
        u /= 10U;
    }

    if(v < 0){
        buf[i++] = '-';
    }

    while(i < w){
        buf[i++] = ' ';
    }

    for(uint8_t a = 0, b = (uint8_t)(i - 1U); a < b; a++, b--){
        char t = buf[a];
        buf[a] = buf[b];
        buf[b] = t;
    }

    buf[i] = '\0';

    lcd_str(buf);
}

/* ============================================================
 * ADC — PA0
 * ============================================================ */
static void adc_init(void){
    GPIOA_CRL &= ~(0xFU << 0);

    ADC1_SMPR2 = (7U << 0);
    ADC1_SQR1 = 0U;
    ADC1_SQR3 = 0U;

    ADC1_CR2 = (7U << 17) | (1U << 20);
    ADC1_CR2 |= (1U << 0);
    delay_ms(5);

    ADC1_CR2 |= (1U << 3);
    while(ADC1_CR2 & (1U << 3));

    ADC1_CR2 |= (1U << 2);
    while(ADC1_CR2 & (1U << 2));
}

static uint16_t adc_read(void){
    ADC1_CR2 |= (1U << 0) | (1U << 22);

    while(!(ADC1_SR & (1U << 1)));

    return (uint16_t)(ADC1_DR & 0xFFFU);
}

static uint16_t adc_avg(void){
    uint32_t s = 0;

    for(uint8_t i = 0; i < 20; i++){
        s += adc_read();
        delay_ms(5);
    }

    return (uint16_t)(s / 20U);
}

/* ============================================================
 * MQ-9 SENSOR MATH
 * ============================================================ */
static float g_R0 = MQ9_R0_DEFAULT;

static float adc_to_rs(uint16_t raw){
    float adc_v = ((float)raw / MQ9_ADC_MAX) * MQ9_VREF;

    /*
     * PA0 voltage is divided voltage.
     * Original MQ AOUT voltage = PA0 voltage * 3.0
     */
    float v = adc_v * ADC_DIVIDER_RATIO;

    if(v < 0.01f){
        v = 0.01f;
    }

    if(v > MQ9_VSUPPLY - 0.01f){
        v = MQ9_VSUPPLY - 0.01f;
    }

    return MQ9_RL * (MQ9_VSUPPLY - v) / v;
}

static float rs_to_ppm(float rs, float A, float B){
    float ratio = rs / g_R0;

    if(ratio < 0.01f){
        ratio = 0.01f;
    }

    if(ratio > 100.0f){
        ratio = 100.0f;
    }

    float ppm = A * powf(ratio, B);

    if(ppm < 0.0f){
        ppm = 0.0f;
    }

    if(ppm > 9999.0f){
        ppm = 9999.0f;
    }

    return ppm;
}

static float read_co_ppm(void){
    return rs_to_ppm(adc_to_rs(adc_avg()), CO_A, CO_B);
}

static float read_lpg_ppm(void){
    return rs_to_ppm(adc_to_rs(adc_avg()), LPG_A, LPG_B);
}

/* Calibrate after 48 hours */
static void calibrate_and_save(void){
    float sum = 0.0f;
    uint8_t count = 0;

    for(uint8_t i = 0; i < 100U; i++){
        float rs = adc_to_rs(adc_read());

        if(rs > 1000.0f && rs < 2000000.0f){
            sum += rs;
            count++;
        }

        delay_ms(100);
    }

    float r0 = (count >= 50U) ? (sum / (float)count) : MQ9_R0_DEFAULT;

    g_R0 = r0;
    flash_save_r0(r0);
}

/* ============================================================
 * LCD SCREENS
 * ============================================================ */
static void screen_warmup(uint32_t done_sec, uint32_t left_sec){
    uint32_t done_h = done_sec / 3600U;
    uint32_t done_m = (done_sec % 3600U) / 60U;

    uint32_t left_h = left_sec / 3600U;
    uint32_t left_m = (left_sec % 3600U) / 60U;

    lcd_pos(0, 0);
    lcd_str("DONE:");
    lcd_int((int32_t)done_h, 2);
    lcd_char('h');

    if(done_m < 10U){
        lcd_char('0');
    }

    lcd_int((int32_t)done_m, 1);
    lcd_char('m');
    lcd_str("   ");

    lcd_pos(0, 1);
    lcd_str("LEFT:");
    lcd_int((int32_t)left_h, 2);
    lcd_char('h');

    if(left_m < 10U){
        lcd_char('0');
    }

    lcd_int((int32_t)left_m, 1);
    lcd_char('m');
    lcd_str("   ");
}

static void screen_ready(uint32_t sec){
    lcd_pos(0, 0);
    lcd_str("  CO MONITOR    ");

    lcd_pos(0, 1);
    lcd_str("READY IN  ");
    lcd_int((int32_t)sec, 3);
    lcd_char('s');
    lcd_str("  ");
}

static void screen_normal(float co, float lpg){
    int32_t c = (co > 9999.0f) ? 9999 : (int32_t)co;
    int32_t l = (lpg > 9999.0f) ? 9999 : (int32_t)lpg;

    lcd_pos(0, 0);
    lcd_str("CO: ");
    lcd_int(c, 4);
    lcd_str(" PPM    ");

    lcd_pos(0, 1);
    lcd_str("LPG:");
    lcd_int(l, 4);
    lcd_str(" PPM    ");
}

static void screen_alarm(float co, float lpg, uint8_t ca, uint8_t la){
    int32_t c = (co > 9999.0f) ? 9999 : (int32_t)co;
    int32_t l = (lpg > 9999.0f) ? 9999 : (int32_t)lpg;

    lcd_pos(0, 0);

    if(ca){
        lcd_str("CO: ");
        lcd_int(c, 4);
        lcd_str("!!!ALRM ");
    }else{
        lcd_str("CO: ");
        lcd_int(c, 4);
        lcd_str(" PPM    ");
    }

    lcd_pos(0, 1);

    if(la){
        lcd_str("LPG:");
        lcd_int(l, 4);
        lcd_str("!!!ALRM ");
    }else{
        lcd_str("LPG:");
        lcd_int(l, 4);
        lcd_str(" PPM    ");
    }
}

/* ============================================================
 * MAIN
 * ============================================================ */
typedef enum {
    STATE_FIRST   = 0,
    STATE_MONITOR = 1,
    STATE_ALARM   = 2
} State_t;

int main(void){
    clock_init();

    RCC_APB2ENR |= (1U << 0) | (1U << 2) | (1U << 3) | (1U << 9);
    RCC_APB1ENR |= (1U << 0);

    AFIO_MAPR = (AFIO_MAPR & ~(7U << 24)) | (2U << 24);

    GPIOA_CRL &= ~(0xFU << 8);
    GPIOA_CRL |=  (0x3U << 8);
    GPIOA_BRR = (1U << 2);

    STK_LOAD_R = 16000U - 1U;
    STK_VAL = 0U;
    STK_CTRL = 7U;

    delay_ms(300);

    i2c_gpio_init();
    adc_init();
    buzzer_off();

    while(1){
        if(i2c_probe(0x27U)){
            g_lcd = 0x27U;
            break;
        }

        delay_ms(100);

        if(i2c_probe(0x3FU)){
            g_lcd = 0x3FU;
            break;
        }

        delay_ms(500);
    }

    lcd_init();
    lcd_clear();

    lcd_pos(0, 0);
    lcd_str("  CO MONITOR    ");
    lcd_pos(0, 1);
    lcd_str("   STARTING...  ");

    buzzer_on();
    delay_ms(300);
    buzzer_off();
    delay_ms(200);

    buzzer_on();
    delay_ms(300);
    buzzer_off();
    delay_ms(500);

    float saved = flash_read_r0();
    State_t state;

    /*
     * FIX: Use only ONE variable for accumulated burn-in time.
     * Do NOT use session_sec + wu_start. Just load from flash
     * and keep adding to this total.
     */
    uint32_t total_burn_sec = 0U;
    uint32_t last_burn_saved_sec = 0U;
    uint32_t last_update_ms = 0U;

    if(saved > 1000.0f && saved < 2000000.0f){
        g_R0 = saved;
        state = STATE_MONITOR;  /* ← SKIP 5-min wait, go straight to monitoring */

        lcd_clear();
        lcd_pos(0, 0);
        lcd_str("  CO MONITOR    ");
        lcd_pos(0, 1);
        lcd_str(" CALIB LOADED   ");
        delay_ms(1500);
        lcd_clear();
    }else{
        g_R0 = MQ9_R0_DEFAULT;
        state = STATE_FIRST;

        /* Load previously saved burn time from flash */
        total_burn_sec = flash_read_burn_seconds();
        last_burn_saved_sec = total_burn_sec;

        lcd_clear();
        lcd_pos(0, 0);
        lcd_str(" FIRST TIME RUN ");
        lcd_pos(0, 1);
        lcd_str(" START:");
        lcd_int((int32_t)(total_burn_sec / 3600U), 2);
        lcd_char('h');
        lcd_int((int32_t)((total_burn_sec % 3600U) / 60U), 2);
        lcd_char('m');
        delay_ms(2000);
    }

    uint32_t last_lcd = g_ms;

    float co_ppm = 0.0f;
    float lpg_ppm = 0.0f;

    while(1){
        if(state == STATE_FIRST){
            buzzer_off();

            /*
             * FIX: Simply increment total_burn_sec each millisecond
             * (at 1000ms = 1 second intervals).
             * No confusing session_sec + wu_start math.
             */
            uint32_t now_ms = g_ms;
            if((now_ms - last_update_ms) >= 1000U){
                last_update_ms = now_ms;
                total_burn_sec++;

                /*
                 * Save progress every 1 minute (60 increments).
                 */
                if((total_burn_sec - last_burn_saved_sec) >= BURN_SAVE_INTERVAL_SEC){
                    flash_save_burn_seconds(total_burn_sec);
                    last_burn_saved_sec = total_burn_sec;
                }
            }

            if((g_ms - last_lcd) >= 1000U){
                last_lcd = g_ms;

                uint32_t rem = (total_burn_sec < WARMUP_FIRST_SEC) ?
                               (WARMUP_FIRST_SEC - total_burn_sec) : 0U;

                screen_warmup(total_burn_sec, rem);
            }

            if(total_burn_sec >= WARMUP_FIRST_SEC){
                flash_save_burn_seconds(WARMUP_FIRST_SEC);

                lcd_clear();
                lcd_pos(0, 0);
                lcd_str(" CALIBRATING... ");
                lcd_pos(0, 1);
                lcd_str("  Keep air clean");

                calibrate_and_save();

                flash_clear_burn_seconds();

                lcd_clear();
                lcd_pos(0, 0);
                lcd_str("  CALIB SAVED!  ");
                lcd_pos(0, 1);
                lcd_str(" No calib again ");

                buzzer_on();
                delay_ms(200);
                buzzer_off();
                delay_ms(150);

                buzzer_on();
                delay_ms(200);
                buzzer_off();
                delay_ms(150);

                buzzer_on();
                delay_ms(200);
                buzzer_off();

                delay_ms(2000);

                lcd_clear();
                state = STATE_MONITOR;
                last_lcd = g_ms;
                last_update_ms = g_ms;
            }
        }

        else if(state == STATE_MONITOR){
            buzzer_off();

            if((g_ms - last_lcd) >= 1000U){
                last_lcd = g_ms;

                co_ppm = read_co_ppm();
                lpg_ppm = read_lpg_ppm();

                screen_normal(co_ppm, lpg_ppm);

                if(co_ppm >= CO_ALARM_PPM || lpg_ppm >= LPG_ALARM_PPM){
                    state = STATE_ALARM;
                }
            }
        }

        else if(state == STATE_ALARM){
            buzzer_on();

            if((g_ms - last_lcd) >= 1000U){
                last_lcd = g_ms;

                co_ppm = read_co_ppm();
                lpg_ppm = read_lpg_ppm();

                uint8_t ca = (co_ppm >= CO_ALARM_PPM) ? 1U : 0U;
                uint8_t la = (lpg_ppm >= LPG_ALARM_PPM) ? 1U : 0U;

                screen_alarm(co_ppm, lpg_ppm, ca, la);

                if(co_ppm < (CO_ALARM_PPM - 5.0f) &&
                   lpg_ppm < (LPG_ALARM_PPM - 50.0f)){
                    buzzer_off();
                    lcd_clear();
                    state = STATE_MONITOR;
                }
            }
        }
    }

    return 0;
    return 1;
}